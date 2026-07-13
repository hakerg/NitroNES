#pragma once
#include <array>
#include "NESConst.h"
#include "A2A03.h"
#include "PPU2C02.h"
#include "Tracer.h"

// TODO: only NTSC is properly supported
enum class NESStandard { NTSC, PAL, DENDY };

class NESCoreBase : public IA2A03 {
public:
    enum class MemoryReadMode { Cpu, External, Peek };

    NESStandard system = NESStandard::NTSC;
    double speed = 1.0;
    bool paused = false;

    explicit NESCoreBase(AudioSettings& audioSettings)
        : a2a03(*this, audioSettings) {
        cpuRam.fill(0x00);
    }
    ~NESCoreBase() override = default;

    double getCPUClockRate()    const { return NES::CPU_CLOCK_NTSC; }
    virtual double getBaseFramerate() const = 0;
    double getTargetFramerate() const { return getBaseFramerate() * speed; }

    void tickFrame() {
        if (paused) return;
        int frames = getCompletedFramesCount();
        do { clockOneCycle(); } while (getCompletedFramesCount() == frames);
        onFrameCompleted();
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

    uint8_t memPeek(uint16_t addr) { return readMemory(addr, MemoryReadMode::Peek); }
    void memWrite(uint16_t addr, uint8_t data) { a2a03.writeExternal(addr, data); }

    int getCurrentScanline() {
        PPU2C02* p = getPPU();
        return p ? p->getScanline() : -1;
    }

protected:
    uint8_t a2a03ReadData(uint16_t addr) override { return readMemory(addr, MemoryReadMode::Cpu); }
    uint8_t a2a03ReadDataExternal(uint16_t addr) override { return readMemory(addr, MemoryReadMode::External); }
    void    a2a03WriteData(uint16_t addr, uint8_t data) override { writeMemory(addr, data); }

    virtual uint8_t readMemory(uint16_t addr, MemoryReadMode mode) = 0;
    virtual void writeMemory(uint16_t addr, uint8_t data) = 0;

    virtual void    clockOneCycle() = 0;
    virtual PPU2C02* getPPU() { return nullptr; }

    virtual void pushAudioSample(float sample, double dt) = 0;
    virtual void onFrameCompleted() {}

    A2A03 a2a03;
    std::array<uint8_t, 2048> cpuRam;
};
