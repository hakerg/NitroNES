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
#include <deque>
#include <thread>

using namespace std::chrono;

enum class CanMatchRefreshRateResult {
    Success,
    Disabled,
    SystemError,
    RefreshRateOutsideTolerance
};

enum class CanUseScanlineSyncResult {
    Success,
    Disabled,
    FileWithNoVideo,
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

    virtual ~IFileSession() = default;

    IFileSession(const IFileSession &) = delete;
    IFileSession &operator=(const IFileSession &) = delete;

    virtual NESCoreBase& getCore() = 0;
    virtual void processKeyDown(AppKey key) {}

    virtual std::string getInfo() = 0;

    virtual int getAudioBufferTargetMs() const { return 15; }

    void saveStateToFile(int slot) {
        auto saveDir = std::filesystem::path(path).parent_path() / "saves";
        std::filesystem::create_directories(saveDir);
        auto filePath = saveDir / (filename + "." + std::to_string(slot) + ".sav");
        std::ofstream ofs(filePath, std::ios::binary);
        if (!ofs) return;
        auto data = getCore().saveState();
        ofs.write(reinterpret_cast<const char*>(data.data()), data.size());
    }

    void loadStateFromFile(int slot) {
        auto saveDir = std::filesystem::path(path).parent_path() / "saves";
        auto filePath = saveDir / (filename + "." + std::to_string(slot) + ".sav");
        std::ifstream ifs(filePath, std::ios::binary);
        if (!ifs) return;
        std::vector<uint8_t> data(std::istreambuf_iterator<char>(ifs), {});
        getCore().loadState(data);
    }

    static bool stateFileExists(const std::string& romPath, int slot) {
        auto saveDir = std::filesystem::path(romPath).parent_path() / "saves";
        auto fileName = std::filesystem::path(romPath).filename().string();
        std::ifstream ifs(saveDir / (fileName + "." + std::to_string(slot) + ".sav"), std::ios::binary);
        return ifs.good();
    }

    void clockCore(double baseSpeed) {
        NESCoreBase& core = getCore();
        core.speed = getSyncedSpeed(baseSpeed);
        if (core.speed < 0) {
            waitUntilNextFrameTime(std::abs(baseSpeed));
            applyRewindFrame();
            return;
        }
        settings.audioSettings.pitch = settings.adjustPitch ? 1.0f : float(1.0 / core.speed);

        int frameMs = static_cast<int>(duration_cast<milliseconds>(getFrameDuration(baseSpeed)).count());
        int bufferTarget = frameMs + getAudioBufferTargetMs();

        int audioQueuedMs = audio.getQueuedMs();
        if (core.paused || audioQueuedMs > bufferTarget) {
            waitUntilNextFrameTime(baseSpeed);
            return;
        }

        if (canUseScanlineSync(baseSpeed) == CanUseScanlineSyncResult::Success) {
            if (baseSpeed < 2) window.delay(1);
            window.pumpEvents();
            nextFrameTime = high_resolution_clock::now();
            syncScanline();
        } else {
            waitUntilNextFrameTime(baseSpeed);
            window.pumpEvents();
            core.tickFrame();
        }
        captureRewindFrame();
    }

    CanMatchRefreshRateResult canMatchRefreshRate(double baseSpeed) {
        if (settings.syncMode == 0) {
            return CanMatchRefreshRateResult::Disabled;
        }

        double monitorHz = window.getRefreshHz();
        if (monitorHz <= 0.0) {
            return CanMatchRefreshRateResult::SystemError;
        }

        double ratio = monitorHz / (getCore().getBaseFramerate() * baseSpeed);
        if (int n = static_cast<int>(std::round(ratio));
            n < 1 || !isWithinDetuneTolerance(ratio / n)) {
            return CanMatchRefreshRateResult::RefreshRateOutsideTolerance;
        }

        return CanMatchRefreshRateResult::Success;
    }

    CanUseScanlineSyncResult canUseScanlineSync(double baseSpeed) {
        if (settings.syncMode != 2) {
            return CanUseScanlineSyncResult::Disabled;
        }

        if (getCore().getCurrentScanline() < 0) {
            return CanUseScanlineSyncResult::FileWithNoVideo;
        }

        int geoW = 0, geoH = 0;
        window.getMonitorGeometry(geoW, geoH);
        if (geoW <= 0 || geoH <= 0) {
            return CanUseScanlineSyncResult::SystemError;
        }

        switch (canMatchRefreshRate(baseSpeed)) {
            case CanMatchRefreshRateResult::Disabled: return CanUseScanlineSyncResult::Disabled;
            case CanMatchRefreshRateResult::SystemError: return CanUseScanlineSyncResult::SystemError;
            case CanMatchRefreshRateResult::RefreshRateOutsideTolerance: return CanUseScanlineSyncResult::RefreshRateOutsideTolerance;
            default: break;
        }

        double monitorHz = window.getRefreshHz();
        if (double ratio = monitorHz / (getCore().getBaseFramerate() * baseSpeed);
            !isWithinDetuneTolerance(ratio)) {
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

    void captureRewindFrame() {
        NESCoreBase& core = getCore();
        auto data = core.saveState();
        if (data.empty()) return;
        if (currBuf.empty()) {
            prevBuf = std::move(data);
            currBuf.resize(prevBuf.size());
            return;
        }
        currBuf = std::move(data);
        std::vector<StateChange> changes;
        for (size_t i = 0; i < prevBuf.size() && i < currBuf.size(); i++) {
            if (prevBuf[i] != currBuf[i])
                changes.push_back({i, prevBuf[i]});
        }
        prevBuf.swap(currBuf);
        rewindHistory.push_back({std::move(changes)});
        while (rewindHistory.size() > 10000)
            rewindHistory.pop_front();
    }

    bool applyRewindFrame() {
        if (rewindHistory.empty()) return false;
        auto frame = std::move(rewindHistory.back());
        rewindHistory.pop_back();
        for (const auto& c : frame.changes) {
            if (c.offset < prevBuf.size())
                prevBuf[c.offset] = c.oldValue;
        }
        getCore().loadState(prevBuf);
        return true;
    }

private:
    static bool isWithinDetuneTolerance(double ratio) {
        constexpr double kCents = 10.0;
        const double upper = std::pow(2.0, kCents / 1200.0);
        const double lower = std::pow(2.0, -kCents / 1200.0);
        return ratio >= lower && ratio <= upper;
    }

    void waitUntilNextFrameTime(double baseSpeed) {
        nanoseconds frameDuration = getFrameDuration(baseSpeed);
        nextFrameTime += frameDuration;
        sleepUntil(nextFrameTime);
    }

    void syncScanline() {
        int monitorScanline = 0;
        window.getScanLine(monitorScanline);

        double monitorHz = window.getRefreshHz();
        int geoW = 0, geoH = 0;
        window.getMonitorGeometry(geoW, geoH);

        double delta = (settings.scanlineBufferMs / 1000.0) * monitorHz * geoH;
        double predicted = monitorScanline + delta;

        float dstX, dstY, dstW, dstH;
        NESCoreBase& core = getCore();
        NES::calcDestRect(geoW, geoH, dstX, dstY, dstW, dstH, core.system);
        float nesY = ((float)predicted - dstY) / dstH * (float)NES::SCREEN_HEIGHT;

        int totalScanlines = core.getTotalScanlines();
        int target = static_cast<int>(std::round(nesY)) % totalScanlines;
        if (target < 0) target += totalScanlines;

        core.tickWhile([&] { return core.getCurrentScanline() != target; });
    }

    nanoseconds getFrameDuration(double baseSpeed) {
        double speed = getSyncedSpeed(baseSpeed);
        double targetFps = getCore().getBaseFramerate() * speed;

        return duration_cast<high_resolution_clock::duration>(
            duration<double>(1.0 / targetFps));
    }

    void sleepUntil(high_resolution_clock::time_point timePoint) {
        auto now = high_resolution_clock::now();
        auto sleepMs = duration_cast<milliseconds>(timePoint - now).count() - 1;
        if (sleepMs >= 0) {
            window.delay(sleepMs);
        }

        while (high_resolution_clock::now() < timePoint) {
            std::this_thread::yield();
        }
    }

    double getSyncedSpeed(double baseSpeed) {
        if (canMatchRefreshRate(baseSpeed) != CanMatchRefreshRateResult::Success) {
            return baseSpeed;
        }

        return baseSpeed * calcSpeedMultiplierForMonitor(baseSpeed);
    }

    double calcSpeedMultiplierForMonitor(double baseSpeed) {
        double monitorHz = window.getRefreshHz();
        double ratio = monitorHz / (getCore().getBaseFramerate() * baseSpeed);
        return ratio / static_cast<int>(std::round(ratio));
    }

    high_resolution_clock::time_point nextFrameTime = high_resolution_clock::now();

    std::deque<RewindFrame> rewindHistory;
    std::vector<uint8_t> prevBuf, currBuf;
};