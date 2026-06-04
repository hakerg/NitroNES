#pragma once
#include "SyncStrategy.h"
#include "PrecisionSleeper.h"
#include <SDL3/SDL.h>

class AudioCallbackSyncStrategy : public SyncStrategy {
public:
	AudioCallbackSyncStrategy(SyncContext& context)
		: SyncStrategy(context)
		, sleeper()
	{}

	void run() override {
		SDL_Event ev;
		while (SDL_PollEvent(&ev)) {
			ctx->handleEvent(ev);
		}

		sleeper.sleep(0.001);
	}

private:
	PrecisionSleeper sleeper;
};
