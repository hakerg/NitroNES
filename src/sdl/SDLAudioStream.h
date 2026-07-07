#pragma once
#include "../AppAudioStream.h"
#include "../AppSettings.h"
#include <SDL3/SDL.h>
#include <iostream>
#include <vector>

class SDLAudioStream : public AppAudioStream {
public:
    explicit SDLAudioStream(AppSettings &settings)
        : AppAudioStream(settings.audioSettings), settings(settings) {
        if (!open())
            std::cerr << "[Audio] Ostrzezenie: brak audio\n";
    }
    ~SDLAudioStream() override { close(); }

    bool isOpen() const { return deviceId != 0 && sdlStream != nullptr; }

    SDL_AudioStream* getSDLStream() const { return sdlStream; }

    AudioBufferHealth getHealth() override {
        int queuedBytes = SDL_GetAudioStreamQueued(sdlStream);
        int bytesPerMs = nativeSpec.freq * nativeSpec.channels * SDL_AUDIO_BYTESIZE(nativeSpec.format) / 1000;
        int queuedMs = queuedBytes / bytesPerMs;

        if (queuedMs < settings.minAudioDelay) {
            return AudioBufferHealth::Underflow;
        }
        if (queuedMs > settings.maxAudioDelay) {
            return AudioBufferHealth::Overflow;
        }
        return AudioBufferHealth::Healthy;
    }

protected:
    void submitSample(float sample) override {
        outBuf.push_back(sample);
        if (outBuf.size() >= batchSize) {
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

        if (!SDL_GetAudioDeviceFormat(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
                                      &nativeSpec, nullptr)) {
            nativeSpec.freq = 48000;
        }

        deviceId = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
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

        batchSize = nativeSpec.freq / 1000;
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
    SDL_AudioSpec nativeSpec{};
    int batchSize = 1;
    AppSettings& settings;
};
