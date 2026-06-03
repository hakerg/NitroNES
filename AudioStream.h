#pragma once
#include <SDL3/SDL.h>
#include <vector>
#include <memory>
#include <iostream>
#include <cmath>

#include "NESConst.h"
#include "AudioFilter.h"
#include "BlipBuffer.h"

// ============================================================
//  AudioStream - BlipBuffer jako bufor audio.
//
//  Watek NES wola:
//    addSample(virtualDt, value)  - buforuje lokalnie w batchBuf
//    commitBatch()                - oproznia BlipBuffer i pushuje
//                                   probki bezposrednio do SDL AudioStream
// ============================================================

class AudioStream {
public:
    static constexpr int CHANNELS = 1;

    AudioStream() = default;
    ~AudioStream() { close(); }

    bool open() {
        if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
            std::cerr << "[Audio] Blad inicjalizacji SDL Audio: " << SDL_GetError() << "\n";
            return false;
        }

        SDL_AudioSpec nativeSpec{};
        if (!SDL_GetAudioDeviceFormat(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &nativeSpec, nullptr)) {
            nativeSpec.freq = 48000;
        }
        sampleRate = nativeSpec.freq;

        deviceId = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
        if (deviceId == 0) {
            std::cerr << "[Audio] Blad otwarcia urzadzenia audio: " << SDL_GetError() << "\n";
            return false;
        }

        if (!SDL_GetAudioDeviceFormat(deviceId, &nativeSpec, nullptr)) {
            std::cerr << "[Audio] Brak informacji o formacie urzadzenia: " << SDL_GetError() << "\n";
            SDL_CloseAudioDevice(deviceId);
            deviceId = 0;
            return false;
        }
        sampleRate = nativeSpec.freq;

        SDL_AudioSpec srcSpec{};
        srcSpec.format   = SDL_AUDIO_F32;
        srcSpec.channels = CHANNELS;
        srcSpec.freq     = sampleRate;

        stream = SDL_CreateAudioStream(&srcSpec, &nativeSpec);
        if (!stream) {
            std::cerr << "[Audio] Blad tworzenia SDL AudioStream: " << SDL_GetError() << "\n";
            SDL_CloseAudioDevice(deviceId);
            deviceId = 0;
            return false;
        }

        if (!SDL_BindAudioStream(deviceId, stream)) {
            std::cerr << "[Audio] Blad bindowania streamu: " << SDL_GetError() << "\n";
            SDL_DestroyAudioStream(stream);
            SDL_CloseAudioDevice(deviceId);
            stream   = nullptr;
            deviceId = 0;
            return false;
        }

        blip = std::make_unique<BlipBuffer>(sampleRate);

        hpf90  = AudioFilter(FilterType::HighPass, NES::AUDIO_HP1_CUTOFF, (float)sampleRate);
        hpf440 = AudioFilter(FilterType::HighPass, NES::AUDIO_HP2_CUTOFF, (float)sampleRate);
        lpf14k = AudioFilter(FilterType::LowPass,  NES::AUDIO_LP_CUTOFF,  (float)sampleRate);

        // Prealokacja buforów: MAX_DELAY * MAX_SPEED pokrywa najgorszy przypadek
        // (4x przyspieszenie + pełny 20ms batch) bez realokacji w hot path.
        const int maxCpuCycles = (int)std::ceil(
            NES::MAX_DELAY * NES::MAX_SPEED * NES::CPU_CLOCK_NTSC) + 16;
        const int maxSamples   = (int)std::ceil(
            NES::MAX_DELAY * NES::MAX_SPEED * sampleRate) + 16;
        batchBuf.reserve(maxCpuCycles);
        outBuf.resize(maxSamples);

        timeAccum    = 0.0;
        batchTimeAcc = 0.0;
        lastEmitted  = 0.0f;

        SDL_ResumeAudioDevice(deviceId);

        return true;
    }

    void close() {
        if (stream) {
            SDL_DestroyAudioStream(stream);
            stream = nullptr;
        }
        if (deviceId) { SDL_CloseAudioDevice(deviceId); deviceId = 0; }
        blip.reset();
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
    }

    // Wola z watku NES - bez locka, tylko buforuje lokalnie.
    void addSample(double virtualDt, float value) {
        batchTimeAcc += virtualDt;
        batchBuf.push_back({ batchTimeAcc, value });
    }

    // Wola z watku NES raz na batch - wpisuje do BlipBuffer,
    // oproznia go i pushuje probki bezposrednio do SDL AudioStream.
    void commitBatch() {
        if (batchBuf.empty() || !blip || !stream) return;

        for (auto& [t, v] : batchBuf)
            blip->addSample(timeAccum + t, v);

        timeAccum += batchTimeAcc;
        batchBuf.clear();
        batchTimeAcc = 0.0;

        // Odczytaj wszystkie dostepne probki z blipa
        int avail = blip->availableSamples(timeAccum);
        if (avail <= 0) return;

        if ((int)outBuf.size() < avail) outBuf.resize(avail);

        int got = blip->readSamples(outBuf.data(), avail, timeAccum);
        if (got <= 0) return;

        timeAccum -= (double)got / sampleRate;
        if (timeAccum < 0.0) timeAccum = 0.0;

        for (int i = 0; i < got; i++) {
            float y = outBuf[i];
            if constexpr (NES::Debug::FILTER_HP90_EN)  y = hpf90.process(y);
            if constexpr (NES::Debug::FILTER_HP440_EN) y = hpf440.process(y);
            if constexpr (NES::Debug::FILTER_LP14K_EN) y = lpf14k.process(y);
            y *= NES::AUDIO_VOLUME;
            outBuf[i] = y;
        }
        lastEmitted = outBuf[got - 1];

        SDL_PutAudioStreamData(stream, outBuf.data(), got * (int)sizeof(float) * CHANNELS);

        // Usuń nadmiar bufora SDL, jeśli przekracza MAX_DELAY
        int queued = SDL_GetAudioStreamQueued(stream);
        int maxBytes = (int)(NES::MAX_DELAY * sampleRate) * (int)sizeof(float) * CHANNELS;
        if (queued > maxBytes) SDL_ClearAudioStream(stream);
    }

    void reset() {
        batchBuf.clear();
        batchTimeAcc = 0.0;
        if (blip) blip->reset();
        hpf90.reset(); hpf440.reset(); lpf14k.reset();
        timeAccum   = 0.0;
        lastEmitted = 0.0f;
        if (stream) SDL_ClearAudioStream(stream);
    }

    bool isOpen()        const { return deviceId != 0 && stream != nullptr; }
    int  getSampleRate() const { return sampleRate; }

private:
    int sampleRate = 0;

    double timeAccum   = 0.0;
    float  lastEmitted = 0.0f;

    SDL_AudioDeviceID deviceId = 0;
    SDL_AudioStream*  stream   = nullptr;

    std::unique_ptr<BlipBuffer> blip;

    AudioFilter hpf90;
    AudioFilter hpf440;
    AudioFilter lpf14k;

    // Lokalny batch watku NES
    struct Entry { double t; float v; };
    std::vector<Entry> batchBuf;
    double             batchTimeAcc = 0.0;

    std::vector<float> outBuf;
};
