#pragma once
#include "SyncStrategy.h"
#include "NESCoreBase.h"
#include "SDLAudioStream.h"
#include "MonitorRefreshRateDetector.h"
#include "NESCoordUtils.h"
#include "PrecisionSleeper.h"
#include <SDL3/SDL.h>
#include <windows.h>
#include <algorithm>
#include <cmath>

class ScanlineSyncStrategy : public SyncStrategy {
public:
	ScanlineSyncStrategy(SyncContext& context)
		: SyncStrategy(context)
		, detector()
		, hwnd(nullptr)
		, sleeper()
	{
		if (context.window) {
			hwnd = (HWND)SDL_GetPointerProperty(SDL_GetWindowProperties(context.window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
			detector.setWindow(hwnd);
		}
	}

	bool canUse() const {
		if (!ctx || !ctx->core->hasPPU()) return false;

		int testScanline = 0;
		if (!detector.getScanLine(testScanline)) return false;

		double monitorRefreshHz = detector.getRefreshHz();
		if (monitorRefreshHz <= 0.0) return false;

		double audioCentsTolerance = 10.0;
		double upperCentsMultiplier = std::pow(2.0, audioCentsTolerance / 1200.0);
		double lowerCentsMultiplier = std::pow(2.0, -audioCentsTolerance / 1200.0);

		double nesRefreshHz = NES::REFRESH_RATE_NTSC_ON * ctx->getSpeed();
		double ratio = monitorRefreshHz / nesRefreshHz;

		return (ratio >= lowerCentsMultiplier && ratio <= upperCentsMultiplier);
	}

	void run() override {
		SDL_Event ev;
		while (SDL_PollEvent(&ev)) {
			ctx->handleEvent(ev);
		}

		int monitorScanline = 0;
		if (ctx->core->isPaused() || !detector.getScanLine(monitorScanline)) {
			sleeper.sleep(0.001);
			return;
		}

		int winW = 0, winH = 0;
		SDL_GetWindowSizeInPixels(ctx->window, &winW, &winH);

		float windowY = (float)monitorScanline;
		float nesX = 0.0f, nesY = 0.0f;
		NES::windowToNES(winW, winH, 0.0f, windowY, nesX, nesY);

		int targetNesScanline = std::clamp((int)nesY, 0, NES::SCREEN_HEIGHT - 1);

		const int SCANLINE_BUFFER = 120;
		targetNesScanline += SCANLINE_BUFFER;
		if (targetNesScanline >= NES::TOTAL_SCANLINES - 1) targetNesScanline -= NES::TOTAL_SCANLINES;

		double speed = ctx->getSpeed();
		double nesRefreshHz = NES::REFRESH_RATE_NTSC_ON * speed;
		double monitorRefreshHz = detector.getRefreshHz();
		ctx->core->setSpeed(monitorRefreshHz > 0.0 ? speed * (monitorRefreshHz / nesRefreshHz) : speed);

		int currentNesScanline = ctx->core->getCurrentScanline();
		while (currentNesScanline != targetNesScanline) {
			ctx->core->tick();
			currentNesScanline = ctx->core->getCurrentScanline();
		}

		ctx->audioStream->commitBatch();
		ctx->core->renderFrame();

		sleeper.sleep(0.001);
	}

private:
	MonitorRefreshRateDetector detector;
	HWND hwnd;
	PrecisionSleeper sleeper;
};



