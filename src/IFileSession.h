#pragma once
#include "AppAudioStream.h"
#include "AppEvent.h"
#include "AppSettings.h"
#include "IWindow.h"
#include "core/NESCoordUtils.h"
#include "core/NESCoreBase.h"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <string>
#include <chrono>
#include <thread>

enum class CanMatchRefreshRateResult {
    Success,
    Disabled,
    SystemError,
    RefreshRateOutsideTolerance
};

enum class CanUseScanlineSyncResult {
    Success,
    Disabled,
    NoFullscreen,
    SystemError,
    RefreshRateOutsideTolerance
};

class IFileSession {
public:
    const std::string path;
    const std::string filename;

    explicit IFileSession(const std::string &path, AppAudioStream &audio,
                          IWindow &window, AppSettings &settings)
        : path(path), filename(std::filesystem::path(path).filename().string()),
          audio(audio), window(window), settings(settings) {
    }

    virtual ~IFileSession() {}

    IFileSession(const IFileSession &) = delete;
    IFileSession &operator=(const IFileSession &) = delete;

    virtual NESCoreBase& getCore() const = 0;

    virtual void processKeyDown(AppKey key) {}

    void clockCore(double baseSpeed) {
        NESCoreBase& core = getCore();
        double adjustedSpeed = getAdjustedSpeed(baseSpeed);
        core.speed = adjustedSpeed;
        settings.audioSettings.pitch = settings.adjustPitch ? 1.0f : float(1.0 / adjustedSpeed);

        if (core.paused || audio.getHealth() == AudioBufferHealth::Overflow) {
            return;
        }

        // do not change 'if' to 'while'
        if (audio.getHealth() == AudioBufferHealth::Underflow) {
            core.tickFrame();
        }

        if (canUseScanlineSync(baseSpeed) == CanUseScanlineSyncResult::Success) {
            syncScanline();
        } else {
            syncTimer(baseSpeed);
        }
    }

    CanMatchRefreshRateResult canMatchRefreshRate(double baseSpeed) const {
        if (settings.syncMode == 0) {
            return CanMatchRefreshRateResult::Disabled;
        }

        double monitorHz = window.getRefreshHz();
        if (monitorHz <= 0.0) {
            return CanMatchRefreshRateResult::SystemError;
        }

        double ratio = monitorHz / (getCore().getBaseFramerate() * baseSpeed);
        int n = static_cast<int>(std::round(ratio));
        if (n < 1 || !isWithinDetuneTolerance(ratio / n)) {
            return CanMatchRefreshRateResult::RefreshRateOutsideTolerance;
        }

        return CanMatchRefreshRateResult::Success;
    }

    CanUseScanlineSyncResult canUseScanlineSync(double baseSpeed) const {
        if (settings.syncMode != 2) {
            return CanUseScanlineSyncResult::Disabled;
        }

        int geoW = 0, geoH = 0;
        window.getMonitorGeometry(geoW, geoH);
        if (geoW <= 0 || geoH <= 0) {
            return CanUseScanlineSyncResult::SystemError;
        }

        CanMatchRefreshRateResult canMatchRefreshRateResult = canMatchRefreshRate(baseSpeed);
        if (canMatchRefreshRateResult != CanMatchRefreshRateResult::Success) {
            switch (canMatchRefreshRateResult) {
                case CanMatchRefreshRateResult::Disabled: return CanUseScanlineSyncResult::Disabled;
                case CanMatchRefreshRateResult::SystemError: return CanUseScanlineSyncResult::SystemError;
                case CanMatchRefreshRateResult::RefreshRateOutsideTolerance: return CanUseScanlineSyncResult::RefreshRateOutsideTolerance;
            }
        }

        double monitorHz = window.getRefreshHz();
        double ratio = monitorHz / (getCore().getBaseFramerate() * baseSpeed);
        if (!isWithinDetuneTolerance(ratio)) {
            return CanUseScanlineSyncResult::RefreshRateOutsideTolerance;
        }

        if (!window.isFullscreen()) {
            return CanUseScanlineSyncResult::NoFullscreen;
        }

        return CanUseScanlineSyncResult::Success;
    }

    uint32_t* getFramebuffer() { return getCore().getFramebuffer(); }

    static bool isNesRomFile(const std::string &path) {
        std::ifstream ifs(path, std::ios::binary);
        if (!ifs)
            return false;
        char m[5] = {};
        ifs.read(m, 5);
        if (ifs.gcount() < 4)
            return false;
        if (m[0] == 'N' && m[1] == 'E' && m[2] == 'S' && m[3] == 'M')
            return false;
        return m[0] == 'N' && m[1] == 'E' && m[2] == 'S' && m[3] == 0x1A;
    }

protected:
    IWindow &window;
    AppSettings &settings;
    AppAudioStream &audio;

private:
    static bool isWithinDetuneTolerance(double ratio) {
        constexpr double kCents = 10.0;
        const double upper = std::pow(2.0, kCents / 1200.0);
        const double lower = std::pow(2.0, -kCents / 1200.0);
        return ratio >= lower && ratio <= upper;
    }

    void syncScanline() {
        window.delay(1);

        int monitorScanline = 0;
        window.getScanLine(monitorScanline);

        double monitorHz = window.getRefreshHz();
        int geoW = 0, geoH = 0;
        window.getMonitorGeometry(geoW, geoH);

        double delta = (settings.scanlineBufferMs / 1000.0) * monitorHz * geoH;
        double predicted = monitorScanline + delta;

        float dstX, dstY, dstW, dstH;
        NES::calcDestRect(geoW, geoH, dstX, dstY, dstW, dstH);
        float nesY = ((float)predicted - dstY) / dstH * (float)NES::SCREEN_HEIGHT;

        int target = static_cast<int>(std::round(nesY)) % NES::TOTAL_SCANLINES;
        if (target < 0) target += NES::TOTAL_SCANLINES;

        NESCoreBase& core = getCore();
        int current = core.getCurrentScanline();

        int distForward = (target - current + NES::TOTAL_SCANLINES) % NES::TOTAL_SCANLINES;
        int distBackward = (current - target + NES::TOTAL_SCANLINES) % NES::TOTAL_SCANLINES;

        // pobieranie scanline monitora może zawierać szum, trzeba się zabezpieczyć przed generowaniem nadmiarowych klatek
        if (distBackward < distForward) {
            return;
        }

        core.tickWhile([&] { return core.getCurrentScanline() != target; });
    }

    void syncTimer(double baseSpeed) {
        using namespace std::chrono;
        NESCoreBase& core = getCore();

        double adjustedSpeed = getAdjustedSpeed(baseSpeed);
        double targetFps = core.getBaseFramerate() * adjustedSpeed;

        auto frameDuration = duration_cast<high_resolution_clock::duration>(
            duration<double>(1.0 / targetFps)
        );

        auto now = high_resolution_clock::now();
        auto targetTime = lastFrameTime + frameDuration;

        if (now - lastFrameTime > milliseconds(30)) {
            targetTime = now;
        }

        sleepUntil(targetTime);

        lastFrameTime = targetTime;
        core.tickFrame();
    }

    void sleepUntil(std::chrono::high_resolution_clock::time_point timePoint) {
        using namespace std::chrono;
        auto now = high_resolution_clock::now();
        auto sleepMs = duration_cast<milliseconds>(timePoint - now).count() - 1;
        if (sleepMs >= 0) {
            window.delay(sleepMs);
        }

        while (high_resolution_clock::now() < timePoint) {
            std::this_thread::yield();
        }
    }

    double getAdjustedSpeed(double baseSpeed) const {
        if (canMatchRefreshRate(baseSpeed) != CanMatchRefreshRateResult::Success) {
            return baseSpeed;
        }

        return baseSpeed * calcSpeedMultiplier(baseSpeed);
    }

    double calcSpeedMultiplier(double baseSpeed) const {
        double monitorHz = window.getRefreshHz();
        double ratio = monitorHz / (getCore().getBaseFramerate() * baseSpeed);
        return ratio / static_cast<int>(std::round(ratio));
    }

    std::chrono::high_resolution_clock::time_point lastFrameTime;
};