#pragma once
#include "SyncStrategy.h"
#include "PrecisionSleeper.h"
#include <SDL3/SDL.h>
#include <mutex>

class AudioCallbackSyncStrategy : public SyncStrategy {
public:
	AudioCallbackSyncStrategy(SyncContext& context)
		: SyncStrategy(context)
		, sleeper()
	{}

	void run() override {
		tick();
		sleeper.sleep(0.001);
	}

private:
	void tick() {
		std::lock_guard<std::mutex> lock(ctx->tickMutex);
		SDL_Event ev;
		while (SDL_PollEvent(&ev)) {
			ctx->handleEvent(ev);
		}
	}

	PrecisionSleeper sleeper;
};
