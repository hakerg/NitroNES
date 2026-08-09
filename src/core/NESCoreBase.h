#pragma once
#include <array>
#include <functional>
#include <vector>
#include "NESConst.h"
#include "A2A03.h"
#include "Tracer.h"

struct StateChange {
    size_t offset;
    uint8_t oldValue;
};

struct RewindFrame {
    std::vector<StateChange> changes;
};

class NESCoreBase : public IA2A03 {
public:
    NESStandard system = NESStandard::NTSC;
    double speed = 1.0;
    bool paused = false;
    std::function<void()> onFrameCapture;

    ~NESCoreBase() override = default;

    double getCPUClockRate() const {
        switch (system) {
        case NESStandard::PAL:   return NES::CPU_CLOCK_PAL;
        case NESStandard::DENDY: return NES::CPU_CLOCK_DENDY;
        default:                 return NES::CPU_CLOCK_NTSC;
        }
    }
    virtual double getBaseFramerate() const = 0;
    double getTargetFramerate() const { return getBaseFramerate() * speed; }

    void setSystem(NESStandard s) {
        if (system == s) return;
        system = s;
        applySystem();
    }

    void tickFrame() {
        if (paused) return;
        int frames = getCompletedFramesCount();
        do { clockOneCycle(); } while (getCompletedFramesCount() == frames);
        onFrameCompleted();
    }

    void tickFrames(uint32_t n) {
        for (uint32_t i = 0; i < n; ++i) tickFrame();
    }

    template <typename ConditionFunc>
    void tickWhile(ConditionFunc condition) {
        if (paused) return;
        int prevFrames = getCompletedFramesCount();
        while (condition()) {
            clockOneCycle();
            int newFrames = getCompletedFramesCount();
            if (newFrames != prevFrames) {
                onFrameCompleted();
                prevFrames = newFrames;
            }
        }
    }

    virtual void reset() {}
    virtual int  getCompletedFramesCount() = 0;
    virtual uint32_t* getFramebuffer() = 0;
    virtual void setTracer(Tracer* t) { a2a03.setTracer(t); }

    virtual std::vector<uint8_t> saveState() const = 0;
    virtual void loadState(const std::vector<uint8_t>& data) = 0;

    virtual uint8_t peekMemory(uint16_t addr) = 0;
    void writeMemory(uint16_t addr, uint8_t data) { a2a03.writeData(addr, data); }

    virtual int getCurrentScanline() = 0;

    virtual int getTotalScanlines() = 0;

    uint8_t a2a03ReadData(uint16_t addr) override { return readMemory(addr); }
    uint8_t a2a03ReadDataExternal(uint16_t addr) override { return readMemoryExternal(addr); }
    void    a2a03WriteData(uint16_t addr, uint8_t data) override { writeMemoryMapped(addr, data); }

protected:
    virtual uint8_t readMemory(uint16_t addr) = 0;
    virtual uint8_t readMemoryExternal(uint16_t addr) = 0;
    virtual void writeMemoryMapped(uint16_t addr, uint8_t data) = 0;

    virtual void clockOneCycle() = 0;

    virtual void applySystem() {}

    virtual void pushAudioSample(float sample, double dt) = 0;

    virtual void onFrameCompleted() {
        if (onFrameCapture) onFrameCapture();
    }

    void pushAudioOutput(float external) {
        pushAudioSample(a2a03.getAPU().getOutputSample() + external,
                        1.0 / (getCPUClockRate() * std::abs(speed)));
    }

    A2A03 a2a03;
    std::array<uint8_t, 2048> cpuRam{};
};
