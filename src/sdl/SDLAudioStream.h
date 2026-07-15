#pragma once
#include "../AppAudioStream.h"
#include "../AppSettings.h"
#include <SDL3/SDL.h>
#include <iostream>
#include <vector>

class SDLAudioStream : public AppAudioStream {
public:
    explicit SDLAudioStream(AppSettings &settings)
        : AppAudioStream(settings.audioSettings) {
        if (!open())
            std::cerr << "[Audio] Ostrzezenie: brak audio\n";
    }
    ~SDLAudioStream() override { close(); }

    bool isOpen() const { return deviceId != 0 && sdlStream != nullptr; }

    SDL_AudioStream* getSDLStream() const { return sdlStream; }

    int getQueuedMs() override {
        if (!isOpen() || nativeSpec.freq <= 0)
            return 0;
        int queuedBytes = SDL_GetAudioStreamQueued(sdlStream);
        if (queuedBytes < 0)
            return 0;
        return static_cast<int>(static_cast<int64_t>(queuedBytes) * 1000 /
                                (nativeSpec.freq * CHANNELS * sizeof(float)));
    }

protected:
    void submitSample(float sample) override {
        if (!isOpen())
            return;
        outBuf.push_back(sample);
        if (outBuf.size() >= batchSize) {
            if (!SDL_PutAudioStreamData(sdlStream, outBuf.data(),
                                       (int)(outBuf.size() * sizeof(float))))
                std::cerr << "[Audio] Blad zapisu do streamu: " << SDL_GetError() << "\n";
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
        outBuf.reserve(batchSize);
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
};
