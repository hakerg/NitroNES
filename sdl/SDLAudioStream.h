#pragma once
#include <SDL3/SDL.h>
#include <iostream>
#include <mutex>
#include <vector>
#include "../AppAudioStream.h"

class SDLAudioStream : public AppAudioStream {
public:
	SDLAudioStream(AudioSettings& settings) : AppAudioStream(settings) {
		if (!open())
			std::cerr << "[Audio] Ostrzezenie: brak audio\n";
	}
	~SDLAudioStream() { close(); }

	bool isOpen() const { return deviceId != 0 && sdlStream != nullptr; }

	SDL_AudioStream* getSDLStream() { return sdlStream; }

protected:
	void submitSample(float sample) override {
		std::lock_guard<std::mutex> lock(outMutex);
		outBuf.push_back(sample);
	}

	void attachSession(IFileSession& s) override {
		session = &s;
		SDL_SetAudioStreamGetCallback(sdlStream, audioCallback, this);
	}

	void detachSession() override {
		SDL_SetAudioStreamGetCallback(sdlStream, nullptr, nullptr);
		session = nullptr;
	}

private:
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
		srcSpec.format = SDL_AUDIO_F32;
		srcSpec.channels = CHANNELS;
		srcSpec.freq = nativeSpec.freq;

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
			deviceId = 0;
			SDL_QuitSubSystem(SDL_INIT_AUDIO);
			return false;
		}

		if (!AudioStream::init(nativeSpec.freq)) {
			SDL_DestroyAudioStream(sdlStream);
			SDL_CloseAudioDevice(deviceId);
			sdlStream = nullptr;
			deviceId = 0;
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

	static void SDLCALL audioCallback(void* userdata, SDL_AudioStream* /*stream*/,
		int additional_amount, int /*total_amount*/) {
		auto* self = static_cast<SDLAudioStream*>(userdata);
		if (!self) return;

		int samplesNeeded = additional_amount / (int)sizeof(float);
		self->flush(samplesNeeded);
	}

	void flush(int samplesNeeded) {
		if (!sdlStream) return;

		while (outBuf.size() < samplesNeeded) {
			if (!session->coreMutex.try_lock()) continue;
			session->core().tickWhile([&] { return outBuf.size() < samplesNeeded; });
			session->coreMutex.unlock();
		}

		std::lock_guard<std::mutex> lock(outMutex);
		SDL_PutAudioStreamData(sdlStream, outBuf.data(), (int)(outBuf.size() * sizeof(float)));
		outBuf.clear();
	}

	IFileSession* session = nullptr;
	SDL_AudioDeviceID deviceId = 0;
	SDL_AudioStream* sdlStream = nullptr;
	std::vector<float> outBuf;
	std::mutex outMutex;
};
