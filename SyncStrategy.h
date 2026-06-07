#pragma once
#include <SDL3/SDL.h>
#include <functional>
#include <mutex>
#include <cmath>
#include "NESConst.h"
#include "NESCoreBase.h"
#include "MonitorRefreshRateDetector.h"

class SyncContext {
private:
    NESCoreBase* core;
    SDL_Window* window;
    MonitorRefreshRateDetector* detector;

    std::function<double()> getBaseSpeed;
    const bool*      matchRefreshRate;

    double calcSpeedMultiplier(bool allowMultiples) const {
        if (!core || !core->hasPPU() || !detector) return 0.0;

        double currentMonitorHz = detector->getRefreshHz();
        if (currentMonitorHz <= 0.0) return 0.0;

        double baseSpeed = getBaseSpeed();
        double nesHz = core->pal ? 50.0 : NES::REFRESH_RATE_NTSC_ON;
        double ratio = currentMonitorHz / (nesHz * baseSpeed);

        int n = allowMultiples ? static_cast<int>(std::round(ratio)) : 1;
        if (n < 1) return 0.0;

        constexpr double audioCentsTolerance = 10.0;
        const double upper = std::pow(2.0, audioCentsTolerance / 1200.0);
        const double lower = std::pow(2.0, -audioCentsTolerance / 1200.0);

        double perMultiple = ratio / n;
        return (perMultiple >= lower && perMultiple <= upper) ? perMultiple : 0.0;
    }

    double getAdjustedSpeed(bool allowMultiples) const {
        double baseSpeed = getBaseSpeed();

        if (!matchRefreshRate || !*matchRefreshRate) return baseSpeed;

        double multiplier = calcSpeedMultiplier(allowMultiples);
        return multiplier > 0.0 ? baseSpeed * multiplier : baseSpeed;
    }

public:
    SyncContext(
        NESCoreBase* c, SDL_Window* w, MonitorRefreshRateDetector* d,
        std::function<double()> spd,
        const bool* match
    ) : core(c), window(w), detector(d),
        getBaseSpeed(spd), matchRefreshRate(match) {
    }

    NESCoreBase* getCore() const { return core; }
    SDL_Window* getWindow() const { return window; }
    MonitorRefreshRateDetector* getDetector() const { return detector; }

    void updateSpeed(bool allowMultipleRefreshRate) {
        if (core) {
            core->speed = getAdjustedSpeed(allowMultipleRefreshRate);
        }
    }

    bool canMatchRefreshRate(bool allowMultiples) const {
        return calcSpeedMultiplier(allowMultiples) > 0.0;
    }
};

class SyncStrategy {
public:
    explicit SyncStrategy(SyncContext& context) : ctx(&context) {}
    virtual ~SyncStrategy() = default;

    virtual void run() = 0;
protected:
    SyncContext* ctx = nullptr;
};