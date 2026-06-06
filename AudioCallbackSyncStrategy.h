#pragma once
#include "SyncStrategy.h"
#include <SDL3/SDL.h>
#include <mutex>

class AudioCallbackSyncStrategy : public SyncStrategy {
public:
	AudioCallbackSyncStrategy(SyncContext& context)
		: SyncStrategy(context)
	{}

	void run() override {
		tick();
		SDL_Delay(1);
	}

private:
	void tick() {
		std::lock_guard<std::mutex> lock(ctx->tickMutex);
		SDL_Event ev;
		while (SDL_PollEvent(&ev)) {
			ctx->handleEvent(ev);
		}
		ctx->core->renderFrame();
	}
};
