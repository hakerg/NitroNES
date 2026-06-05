#pragma once
#include "SyncStrategy.h"
#include "NESCoreBase.h"
#include "PrecisionSleeper.h"
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
		, sleeper()
	{}

	void run() override {
		tick();
		sleeper.sleep(0.001);
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
		ctx->core->setSpeed(ctx->getSpeed());
		while (lag > 0.0) {
			double dt = ctx->core->tickFrame();
			lag -= dt;
		}
		ctx->core->renderFrame();
	}
	Uint64 freq;
	Uint64 prev;
	double lag;
	double maxLag;
	PrecisionSleeper sleeper;
};
