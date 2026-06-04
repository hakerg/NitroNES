#pragma once
#include <SDL3/SDL.h>
#include <iostream>

#include "AudioStream.h"

// ============================================================
//  SDLAudioStream - backend audio oparty na SDL3.
//
//  Dziedziczy po AudioStream i implementuje submitSamples(),
//  przekazujac gotowe probki float32 mono do SDL AudioStream.
// ============================================================

class SDLAudioStream : public AudioStream {
public:
	SDLAudioStream() = default;
	~SDLAudioStream() { close(); }

	bool open() {
		if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
			std::cerr << "[Audio] Blad inicjalizacji SDL Audio: " << SDL_GetError() << "\n";
			return false;
		}

		SDL_AudioSpec nativeSpec{};
		if (!SDL_GetAudioDeviceFormat(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &nativeSpec, nullptr)) {
			nativeSpec.freq = 48000;
		}

		deviceId = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
		if (deviceId == 0) {
			std::cerr << "[Audio] Blad otwarcia urzadzenia audio: " << SDL_GetError() << "\n";
			SDL_QuitSubSystem(SDL_INIT_AUDIO);
			return false;
		}

		if (!SDL_GetAudioDeviceFormat(deviceId, &nativeSpec, nullptr)) {
			std::cerr << "[Audio] Brak informacji o formacie urzadzenia: " << SDL_GetError() << "\n";
			SDL_CloseAudioDevice(deviceId);
			deviceId = 0;
			SDL_QuitSubSystem(SDL_INIT_AUDIO);
			return false;
		}

		SDL_AudioSpec srcSpec{};
		srcSpec.format   = SDL_AUDIO_F32;
		srcSpec.channels = CHANNELS;
		srcSpec.freq     = nativeSpec.freq;

		sdlStream = SDL_CreateAudioStream(&srcSpec, &nativeSpec);
		if (!sdlStream) {
			std::cerr << "[Audio] Blad tworzenia SDL AudioStream: " << SDL_GetError() << "\n";
			SDL_CloseAudioDevice(deviceId);
			deviceId = 0;
			SDL_QuitSubSystem(SDL_INIT_AUDIO);
			return false;
		}

		if (!SDL_BindAudioStream(deviceId, sdlStream)) {
			std::cerr << "[Audio] Blad bindowania streamu: " << SDL_GetError() << "\n";
			SDL_DestroyAudioStream(sdlStream);
			SDL_CloseAudioDevice(deviceId);
			sdlStream = nullptr;
			deviceId  = 0;
			SDL_QuitSubSystem(SDL_INIT_AUDIO);
			return false;
		}

		if (!AudioStream::init(nativeSpec.freq)) {
			SDL_DestroyAudioStream(sdlStream);
			SDL_CloseAudioDevice(deviceId);
			sdlStream = nullptr;
			deviceId  = 0;
			SDL_QuitSubSystem(SDL_INIT_AUDIO);
			return false;
		}

		SDL_ResumeAudioDevice(deviceId);
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

	bool isOpen() const { return deviceId != 0 && sdlStream != nullptr; }

protected:
	void submitSamples(const float* samples, int count) override {
		if (!sdlStream) return;

		SDL_PutAudioStreamData(sdlStream, samples, count * (int)sizeof(float) * CHANNELS);

		// Usuń nadmiar bufora SDL, jesli przekracza MAX_DELAY
		int queued   = SDL_GetAudioStreamQueued(sdlStream);
		int maxBytes = (int)(NES::MAX_DELAY * getSampleRate()) * (int)sizeof(float) * CHANNELS;
		if (queued > maxBytes) SDL_ClearAudioStream(sdlStream);
	}

private:
	SDL_AudioDeviceID deviceId  = 0;
	SDL_AudioStream*  sdlStream = nullptr;
};
