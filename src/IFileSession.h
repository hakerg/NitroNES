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

    // Zwraca true gdy scanline sync jest włączony i możliwy do użycia.
    bool canUseScanlineSync(double baseSpeed) const {
        int tmp = 0;
        return baseSpeed == 1.0 && settings.allowScanlineSync &&
               window.getScanLine(tmp);
    }

    // Zwraca mnożnik dopasowania do monitora (np. 0.997), lub 0 gdy niemożliwe.
    // n=1 przy scanline sync, n=round(ratio) przy timer sync.
    double calcSpeedMultiplier(double baseSpeed) const {
        if (!core().hasPPU())
            return 0.0;
        double monitorHz = window.getRefreshHz();
        if (monitorHz <= 0.0)
            return 0.0;

        double nesHz =
            core().pal ? 50.0
                       : NES::REFRESH_RATE_NTSC_ON; // TODO: proper pal support
        double ratio = monitorHz / (nesHz * baseSpeed);

        int n = canUseScanlineSync(baseSpeed)
                    ? 1
                    : static_cast<int>(std::round(ratio));
        if (n < 1)
            return 0.0;

        constexpr double kCents = 10.0;
        double upper = std::pow(2.0, kCents / 1200.0);
        double lower = std::pow(2.0, -kCents / 1200.0);
        double per = ratio / n;
        return (per >= lower && per <= upper) ? per : 0.0;
    }

    double adjustedSpeed(double baseSpeed) const {
        if (!settings.matchRefreshRate)
            return baseSpeed;
        double m = calcSpeedMultiplier(baseSpeed);
        return m > 0.0 ? baseSpeed * m : baseSpeed;
    }

    void updateSpeed(double baseSpeed) {
        core().speed = adjustedSpeed(baseSpeed);
        settings.audioSettings.pitch = (float)(1.0 / core().speed);
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
};
