#pragma once
#include "SyncStrategy.h"
#include "NESCoreBase.h"
#include "SDLAudioStream.h"
#include "PrecisionSleeper.h"
#include <SDL3/SDL.h>

class AudioCallbackSyncStrategy : public SyncStrategy {
public:
	AudioCallbackSyncStrategy(SyncContext& context)
		: SyncStrategy(context)
		, sleeper()
	{
		if (!context.core->hasPPU()) {
			SDL_AudioStream* stream = context.audioStream->getSDLStream();
			if (stream) {
				SDL_SetAudioStreamGetCallback(stream, audioCallback, this);
			}
		}
	}

	~AudioCallbackSyncStrategy() {
		SDL_AudioStream* stream = ctx->audioStream->getSDLStream();
		if (stream) {
			SDL_SetAudioStreamGetCallback(stream, nullptr, nullptr);
		}
	}

	void run() override {
		SDL_Event ev;
		while (SDL_PollEvent(&ev)) {
			ctx->handleEvent(ev);
		}

		sleeper.sleep(0.001);
	}

private:
	static void SDLCALL audioCallback(void* userdata, SDL_AudioStream* stream, int additional_amount, int total_amount) {
		auto* self = static_cast<AudioCallbackSyncStrategy*>(userdata);
		if (!self || !self->ctx) return;

		SyncContext& ctx = *self->ctx;

		int samplesNeeded = additional_amount / ((int)sizeof(float) * SDLAudioStream::CHANNELS);
		if (samplesNeeded <= 0) return;

		ctx.core->setSpeed(ctx.getSpeed());

		while (ctx.audioStream->getQueuedSamples() < samplesNeeded && !ctx.core->isPaused()) {
			ctx.core->tick();
		}

		ctx.audioStream->commitBatch();
	}

	PrecisionSleeper sleeper;
};
