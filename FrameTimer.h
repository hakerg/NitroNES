#pragma once
#include <SDL3/SDL.h>

// Mierzy czas między klatkami i akumuluje opóźnienie (lag).
class FrameTimer {
public:
	explicit FrameTimer(double maxDelay)
		: freq(SDL_GetPerformanceFrequency())
		, prev(SDL_GetPerformanceCounter())
		, lag(0.0)
		, maxDelay(maxDelay)
	{}

	// Zwraca true, gdy są jeszcze ticki do wykonania.
	// Gdy lag spadł do zera, automatycznie pobiera czas z licznika i doładowuje lag.
	bool shouldTick() {
		if (lag > 0.0) return true;
		Uint64 now = SDL_GetPerformanceCounter();
		double elapsed = (double)(now - prev) / (double)freq;
		prev = now;
		lag += elapsed;
		if (lag > maxDelay) lag = maxDelay;
		return lag > 0.0;
	}

	// Odejmuje dt od zakumulowanego lagu po wykonaniu jednego ticku.
	void addTime(double dt) {
		lag -= dt;
	}

private:
	Uint64 freq;
	Uint64 prev;
	double lag;
	double maxDelay;
};
