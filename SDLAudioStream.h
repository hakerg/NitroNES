#pragma once
#include <SDL3/SDL.h>
#include <iostream>
#include <mutex>
#include <vector>

#include "AudioStream.h"
#include "SyncStrategy.h"

// ============================================================
//  SDLAudioStream - backend audio oparty na SDL3.
//
//  Dziedziczy po AudioStream i implementuje submitSample(),
//  ktore pushuje kazda probke float32 mono bezposrednio do SDL.
//
//  Po wywolaniu bindContext() rejestruje universalny audioCallback,
//  ktory sam dotacza tickow gdy bufor SDL jest za maly.
//  Synchronizacja przez SyncContext::tickMutex.
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

	void bindContext(SyncContext* ctx) {
		syncCtx = ctx;
		if (sdlStream) {
			SDL_SetAudioStreamGetCallback(sdlStream, audioCallback, this);
		}
	}

	void close() {
		if (sdlStream) {
			SDL_SetAudioStreamGetCallback(sdlStream, nullptr, nullptr);
			SDL_DestroyAudioStream(sdlStream);
			sdlStream = nullptr;
		}
		if (deviceId) {
			SDL_CloseAudioDevice(deviceId);
			deviceId = 0;
			SDL_QuitSubSystem(SDL_INIT_AUDIO);
		}
		syncCtx = nullptr;
	}

	bool isOpen() const { return deviceId != 0 && sdlStream != nullptr; }

	SDL_AudioStream* getSDLStream() { return sdlStream; }

protected:
	void submitSample(float sample) override {
		int maxSamples = (int)(NES::MAX_LAG * getSampleRate());
		if ((int)outBuf.size() < maxSamples) {
			outBuf.push_back(sample);
		}
	}

private:
	static void SDLCALL audioCallback(void* userdata, SDL_AudioStream* /*stream*/,
									   int additional_amount, int /*total_amount*/) {
		auto* self = static_cast<SDLAudioStream*>(userdata);
		if (!self || !self->syncCtx) return;

		SyncContext& ctx = *self->syncCtx;

		int samplesNeeded = additional_amount / (int)sizeof(float);
		if (samplesNeeded <= 0) return;

		std::lock_guard<std::mutex> lock(ctx.tickMutex);
		if (!ctx.core->isPaused()) {
			ctx.core->setSpeed(ctx.getSpeed());
			while ((int)self->outBuf.size() < samplesNeeded) {
				ctx.core->tick();
			}
		}
		self->flush();
	}

	void flush() {
		if (outBuf.empty() || !sdlStream) return;

		SDL_PutAudioStreamData(sdlStream, outBuf.data(), (int)(outBuf.size() * sizeof(float)));
		outBuf.clear();
	}

	SDL_AudioDeviceID    deviceId  = 0;
	SDL_AudioStream*     sdlStream = nullptr;
	SyncContext*         syncCtx   = nullptr;
	std::vector<float>   outBuf;
};
