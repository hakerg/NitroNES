#pragma once
#include "AppAudioStream.h"
#include "AppEvent.h"
#include "AppSettings.h"
#include "IWindow.h"
#include "core/NESCoreBase.h"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>

enum class CanMatchRefreshRateResult {
    Success,
    Disabled,
    NoRendering,
    SystemError,
    RefreshRateOutsideTolerance
};

enum class CanUseScanlineSyncResult {
    Success,
    Disabled,
    NoRendering,
    NoFullscreen,
    SystemError,
    RefreshRateOutsideTolerance
};

class IFileSession {
public:
    const std::string path;
    const std::string filename;
    std::mutex coreMutex;

    explicit IFileSession(const std::string &path, AppAudioStream &audio,
                          IWindow &window, AppSettings &settings)
        : path(path), filename(std::filesystem::path(path).filename().string()),
          audio(audio), window(window), settings(settings) {
        audio.attachSession(*this);
    }

    virtual ~IFileSession() {}

    IFileSession(const IFileSession &) = delete;
    IFileSession &operator=(const IFileSession &) = delete;

    virtual NESCoreBase &core() const = 0;

    virtual void processKeyDown(AppKey key) {}

    virtual void runFrame(double baseSpeed) = 0;

    CanMatchRefreshRateResult canMatchRefreshRate(double baseSpeed) const {
        if (settings.syncMode == 0) {
            return CanMatchRefreshRateResult::Disabled;
        }

        if (!core().hasPPU()) {
            return CanMatchRefreshRateResult::NoRendering;
        }

        double monitorHz = window.getRefreshHz();
        if (monitorHz <= 0.0) {
            return CanMatchRefreshRateResult::SystemError;
        }

        double ratio = monitorHz / (core().getBaseFramerate() * baseSpeed);
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
                case CanMatchRefreshRateResult::NoRendering: return CanUseScanlineSyncResult::NoRendering;
                case CanMatchRefreshRateResult::SystemError: return CanUseScanlineSyncResult::SystemError;
                case CanMatchRefreshRateResult::RefreshRateOutsideTolerance: return CanUseScanlineSyncResult::RefreshRateOutsideTolerance;
            }
        }

        double monitorHz = window.getRefreshHz();
        double ratio = monitorHz / (core().getBaseFramerate() * baseSpeed);
        if (!isWithinDetuneTolerance(ratio)) {
            return CanUseScanlineSyncResult::RefreshRateOutsideTolerance;
        }

        if (!window.isFullscreen()) {
            return CanUseScanlineSyncResult::NoFullscreen;
        }

        return CanUseScanlineSyncResult::Success;
    }

    double calcSpeedMultiplier(double baseSpeed) const {
        double monitorHz = window.getRefreshHz();
        double ratio = monitorHz / (core().getBaseFramerate() * baseSpeed);
        return ratio / static_cast<int>(std::round(ratio));
    }

    double adjustedSpeed(double baseSpeed) const {
        if (canMatchRefreshRate(baseSpeed) != CanMatchRefreshRateResult::Success) {
            return baseSpeed;
        }

        return baseSpeed * calcSpeedMultiplier(baseSpeed);
    }

    void updateSpeed(double baseSpeed) {
        core().speed = adjustedSpeed(baseSpeed);
        settings.audioSettings.pitch = settings.adjustPitch ? 1.0f : float(1.0 / core().speed);
    }

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

    static bool isWithinDetuneTolerance(double ratio) {
        constexpr double kCents = 10.0;
        const double upper = std::pow(2.0, kCents / 1200.0);
        const double lower = std::pow(2.0, -kCents / 1200.0);
        return ratio >= lower && ratio <= upper;
    }
};