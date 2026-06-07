#pragma once
#include "SyncStrategy.h"
#include "NESCoordUtils.h"
#include <windows.h>
#include <algorithm>

class ScanlineSyncStrategy : public SyncStrategy {
public:
    ScanlineSyncStrategy(SyncContext& context, int* bufferMs = nullptr)
        : SyncStrategy(context)
        , hwnd(nullptr)
        , scanlineBufferMs(bufferMs)
    {
        if (context.getWindow() && context.getDetector()) {
            hwnd = (HWND)SDL_GetPointerProperty(SDL_GetWindowProperties(context.getWindow()), SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
            context.getDetector()->setWindow(hwnd);
        }
    }

    bool canUse() const {
        if (!ctx || !ctx->getDetector()) return false;
        int testScanline = 0;
        if (!ctx->getDetector()->getScanLine(testScanline)) return false;

        return ctx->canMatchRefreshRate(false);
    }

    void run() override {
        int targetNesScanline = 0;

        int monitorScanline = 0;
        if (!ctx->getDetector()->getScanLine(monitorScanline)) return;

        int winW = 0, winH = 0;
        SDL_GetWindowSizeInPixels(ctx->getWindow(), &winW, &winH);

        int renderAreaX = 0, renderAreaY = 0;
        getWindowRenderPosition(renderAreaX, renderAreaY);

        HMONITOR hmon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi{};
        mi.cbSize = sizeof(mi);
        GetMonitorInfoW(hmon, &mi);
        int monitorHeight = mi.rcMonitor.bottom - mi.rcMonitor.top;

        double monitorRefreshHz = ctx->getDetector()->getRefreshHz();

        if (monitorHeight <= 0 || monitorRefreshHz <= 0.0) return;

        double bufMs = (scanlineBufferMs && *scanlineBufferMs >= 0) ? *scanlineBufferMs : 6;
        double monitorScanlinesDelta = (bufMs / 1000.0) * monitorRefreshHz * monitorHeight;
        double predictedMonitorScanline = monitorScanline + monitorScanlinesDelta;

        float nesX = 0.0f, nesY = 0.0f;
        NES::monitorToNES(renderAreaX, renderAreaY, winW, winH, 0.0f, predictedMonitorScanline, nesX, nesY);

        targetNesScanline = static_cast<int>(std::round(nesY));
        targetNesScanline %= NES::TOTAL_SCANLINES;
        if (targetNesScanline < 0) targetNesScanline += NES::TOTAL_SCANLINES;

        NESCoreBase* core = ctx->getCore();

        ctx->updateSpeed(false);
        core->tickWhile([&]() { return core->getCurrentScanline() != targetNesScanline; });

        core->renderFrame();
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

    HWND hwnd;
    int* scanlineBufferMs;
};