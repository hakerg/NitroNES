#pragma once
#include "SyncStrategy.h"
#include "NESCoreBase.h"
#include <SDL3/SDL.h>
#include <mutex>

class TimerSyncStrategy : public SyncStrategy {
public:
	TimerSyncStrategy(SyncContext& context, double maxLag)
		: SyncStrategy(context)
		, freq(SDL_GetPerformanceFrequency())
		, prev(SDL_GetPerformanceCounter())
		, lag(0.0)
		, maxLag(maxLag)
	{}

	void run() override {
		tick();
		SDL_Delay(1);
	}

private:
	void tick() {
		Uint64 now = SDL_GetPerformanceCounter();
		double elapsed = (double)(now - prev) / (double)freq;
		prev = now;
		lag += elapsed;
		if (lag > maxLag) lag = maxLag;

		std::lock_guard<std::mutex> lock(ctx->tickMutex);
		SDL_Event ev;
		while (SDL_PollEvent(&ev)) {
			ctx->handleEvent(ev);
		}
		if (ctx->core->isPaused()) return;
		ctx->core->setSpeed(ctx->getSpeed());
		while (lag > 0.0) {
			double dt = ctx->core->tickFrame();
			lag -= dt;
			ctx->core->renderFrame();
		}
	}

	Uint64 freq;
	Uint64 prev;
	double lag;
	double maxLag;
};
