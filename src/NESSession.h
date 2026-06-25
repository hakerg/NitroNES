#pragma once
#include "AppSettings.h"
#include "IFileSession.h"
#include "IInputContext.h"
#include "IWindow.h"
#include "core/NESConst.h"
#include "core/NESCoordUtils.h"
#include "core/NESSystem.h"
#include <chrono>
#include <cmath>

class NESSession : public IFileSession, public INESSystemHost {
public:
    NESSession(const std::string &path, IInputContext &input, IWindow &window,
               AppSettings &settings, IEmulatorHost &host,
               AppAudioStream &audio)
        : IFileSession(path, audio, window, settings),
          nes(host, *this, settings.audioSettings, path),
          input(input) {}

    ~NESSession() override {
        std::lock_guard lock(coreMutex);
        audio.detachSession();
    }

    NESCoreBase &core() const override { return nes; }

    void runFrame(double baseSpeed) override {
        if (canUseScanlineSync(baseSpeed) == CanUseScanlineSyncResult::Success) {
            runScanlineSync(baseSpeed);
        } else {
            runTimerSync(baseSpeed);
        }

        window.presentNESFrame(nes.getFramebuffer(), *this, baseSpeed);
    }

    uint8_t readController(int port) override {
        return input.readController(port);
    }

private:
    void runTimerSync(double baseSpeed) {
        auto now = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(now - timerPrev).count();
        timerPrev = now;
        timerLag += elapsed;
        if (timerLag > MAX_LAG) timerLag = MAX_LAG;

        if (nes.paused) {
            return;
        }

        updateSpeed(baseSpeed);
        while (timerLag > 0.0) {
            double dt;
            std::lock_guard lock(coreMutex);
            nes.tickFrame(dt);
            timerLag -= dt;
        }
    }

    void runScanlineSync(double baseSpeed) {
        int monitorScanline = 0;
        window.getScanLine(monitorScanline);

        double monitorHz = window.getRefreshHz();
        int geoW = 0, geoH = 0;
        window.getMonitorGeometry(geoW, geoH);

        double delta = (settings.scanlineBufferMs / 1000.0) * monitorHz * geoH;
        double predicted = monitorScanline + delta;

        float dstX, dstY, dstW, dstH;
        NES::calcDestRect(geoW, geoH, dstX, dstY, dstW, dstH);
        float nesY = ((float)predicted - dstY) / dstH * (float)NES::VISIBLE_H +
                     (float)NES::OVERSCAN_TOP;

        int target = static_cast<int>(std::round(nesY)) % NES::TOTAL_SCANLINES;
        if (target < 0) target += NES::TOTAL_SCANLINES;

        updateSpeed(baseSpeed);
        std::lock_guard lock(coreMutex);
        nes.tickWhile([&] { return nes.getCurrentScanline() != target; });
    }

    static constexpr double MAX_LAG = 0.02;

    mutable NESSystem nes;
    IInputContext &input;

    std::chrono::steady_clock::time_point timerPrev =
        std::chrono::steady_clock::now();
    double timerLag = 0.0;
};