#pragma once
#include "../AppAudioStream.h"
#include <SDL3/SDL.h>
#include <iostream>
#include <vector>

class SDLAudioStream : public AppAudioStream {
public:
    static const int BATCH_SIZE = 256;

    explicit SDLAudioStream(AudioSettings &settings)
        : AppAudioStream(settings) {
        if (!open())
            std::cerr << "[Audio] Ostrzezenie: brak audio\n";
    }
    ~SDLAudioStream() { close(); }

    bool isOpen() const { return deviceId != 0 && sdlStream != nullptr; }

    SDL_AudioStream* getSDLStream() const { return sdlStream; }

    AudioBufferHealth getHealth() override {
        SDL_AudioSpec dstSpec{};
        SDL_GetAudioStreamFormat(sdlStream, nullptr, &dstSpec);

        int queuedBytes = SDL_GetAudioStreamQueued(sdlStream);
        float bytesPerMs = dstSpec.freq * dstSpec.channels * SDL_AUDIO_BYTESIZE(dstSpec.format) * 0.001f;
        float queuedMs = queuedBytes / bytesPerMs;

        const float MIN_SAFE_MS = 20.0f;
        const float MAX_SAFE_MS = 40.0f;

        if (queuedMs < MIN_SAFE_MS) {
            return AudioBufferHealth::Underflow;
        }
        if (queuedMs > MAX_SAFE_MS) {
            return AudioBufferHealth::Overflow;
        }
        return AudioBufferHealth::Healthy;
    }

protected:
    void submitSample(float sample) override {
        outBuf.push_back(sample);
        if (outBuf.size() >= BATCH_SIZE) {
            SDL_PutAudioStreamData(sdlStream, outBuf.data(),
                                   (int)(outBuf.size() * sizeof(float)));
            outBuf.clear();
        }
    }

private:
    bool open() {
        if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
            std::cerr << "[Audio] Blad inicjalizacji SDL Audio: "
                      << SDL_GetError() << "\n";
            return false;
        }

        SDL_AudioSpec nativeSpec{};
        if (!SDL_GetAudioDeviceFormat(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
                                      &nativeSpec, nullptr)) {
            nativeSpec.freq = 48000;
        }

        deviceId =
            SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
        if (deviceId == 0) {
            std::cerr << "[Audio] Blad otwarcia urzadzenia audio: "
                      << SDL_GetError() << "\n";
            SDL_QuitSubSystem(SDL_INIT_AUDIO);
            return false;
        }

        if (!SDL_GetAudioDeviceFormat(deviceId, &nativeSpec, nullptr)) {
            std::cerr << "[Audio] Brak informacji o formacie urzadzenia: "
                      << SDL_GetError() << "\n";
            SDL_CloseAudioDevice(deviceId);
            deviceId = 0;
            SDL_QuitSubSystem(SDL_INIT_AUDIO);
            return false;
        }

        SDL_AudioSpec srcSpec{};
        srcSpec.format = SDL_AUDIO_F32;
        srcSpec.channels = CHANNELS;
        srcSpec.freq = nativeSpec.freq;

        sdlStream = SDL_CreateAudioStream(&srcSpec, &nativeSpec);
        if (!sdlStream) {
            std::cerr << "[Audio] Blad tworzenia SDL AudioStream: "
                      << SDL_GetError() << "\n";
            SDL_CloseAudioDevice(deviceId);
            deviceId = 0;
            SDL_QuitSubSystem(SDL_INIT_AUDIO);
            return false;
        }

        if (!SDL_BindAudioStream(deviceId, sdlStream)) {
            std::cerr << "[Audio] Blad bindowania streamu: " << SDL_GetError()
                      << "\n";
            SDL_DestroyAudioStream(sdlStream);
            SDL_CloseAudioDevice(deviceId);
            sdlStream = nullptr;
            deviceId = 0;
            SDL_QuitSubSystem(SDL_INIT_AUDIO);
            return false;
        }

        init(nativeSpec.freq);
        return true;
    }

    void close() {
        if (sdlStream) {
            SDL_DestroyAudioStream(sdlStream);
            sdlStream = nullptr;
        }
        if (deviceId) {
            SDL_CloseAudioDevice(deviceId);
            deviceId = 0;
            SDL_QuitSubSystem(SDL_INIT_AUDIO);
        }
    }

    SDL_AudioDeviceID deviceId = 0;
    SDL_AudioStream* sdlStream = nullptr;
    std::vector<float> outBuf;
};
