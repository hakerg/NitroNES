#pragma once
#include "SyncStrategy.h"
#include "NESCoreBase.h"
#include "MonitorRefreshRateDetector.h"
#include "NESCoordUtils.h"
#include <SDL3/SDL.h>
#include <windows.h>
#include <algorithm>
#include <cmath>
#include <mutex>

class ScanlineSyncStrategy : public SyncStrategy {
public:
	ScanlineSyncStrategy(SyncContext& context)
		: SyncStrategy(context)
		, detector()
		, hwnd(nullptr)
	{
		if (context.window) {
			hwnd = (HWND)SDL_GetPointerProperty(SDL_GetWindowProperties(context.window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
			detector.setWindow(hwnd);
		}
	}

	bool canUse() const {
		if (!ctx || !ctx->core->hasPPU() || detector.isDuplicateMode()) return false;

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
		tick();
		SDL_Delay(1);
	}

private:
	void getWindowRenderPosition(int& outRenderAreaX, int& outRenderAreaY) {
		POINT pt = { 0, 0 };
		ClientToScreen(hwnd, &pt);

		HMONITOR hmon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
		MONITORINFO mi{};
		mi.cbSize = sizeof(mi);
		GetMonitorInfoW(hmon, &mi);

		outRenderAreaX = pt.x - mi.rcMonitor.left;
		outRenderAreaY = pt.y - mi.rcMonitor.top;
	}

	void tick() {
		int monitorScanline = 0;
		if (!detector.getScanLine(monitorScanline)) return;

		int winW = 0, winH = 0;
		SDL_GetWindowSizeInPixels(ctx->window, &winW, &winH);

		int renderAreaX = 0, renderAreaY = 0;
		getWindowRenderPosition(renderAreaX, renderAreaY);

		HMONITOR hmon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
		MONITORINFO mi{};
		mi.cbSize = sizeof(mi);
		GetMonitorInfoW(hmon, &mi);
		int monitorHeight = mi.rcMonitor.bottom - mi.rcMonitor.top;

		double monitorRefreshHz = detector.getRefreshHz();

		if (monitorHeight <= 0 || monitorRefreshHz <= 0.0) return;

		double monitorScanlinesDelta = 0.006 * monitorRefreshHz * monitorHeight;
		double predictedMonitorScanline = monitorScanline + monitorScanlinesDelta;

		float nesX = 0.0f, nesY = 0.0f;
		NES::monitorToNES(renderAreaX, renderAreaY, winW, winH, 0.0f, predictedMonitorScanline, nesX, nesY);

		int targetNesScanline = static_cast<int>(std::round(nesY));

		targetNesScanline %= NES::TOTAL_SCANLINES;
		if (targetNesScanline < 0) {
			targetNesScanline += NES::TOTAL_SCANLINES;
		}

		double speed = ctx->getSpeed();
		double nesRefreshHz = NES::REFRESH_RATE_NTSC_ON * speed;

		std::lock_guard<std::mutex> lock(ctx->tickMutex);
		SDL_Event ev;
		while (SDL_PollEvent(&ev)) {
			ctx->handleEvent(ev);
		}

		if (ctx->core->isPaused()) return;

		ctx->core->setSpeed(speed * (monitorRefreshHz / nesRefreshHz));
		int currentNesScanline = ctx->core->getCurrentScanline();

		while (currentNesScanline != targetNesScanline) {
			ctx->core->tick();
			currentNesScanline = ctx->core->getCurrentScanline();
		}
		ctx->core->renderFrame();
	}

	MonitorRefreshRateDetector detector;
	HWND hwnd;
};
