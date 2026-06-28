#pragma once
#include <array>
#include "NESConst.h"
#include "A2A03.h"
#include "PPU2C02.h"

// TODO: czy można te interfejsy usunąć?
class IEmulatorHost {
public:
    virtual ~IEmulatorHost() = default;
    virtual void pushAudioSample(float sample, double dt) {}
    virtual void onFrameReady() {}
};

// TODO: only NTSC is properly supported
enum class NESStandard { NTSC, PAL, DENDY };

class NESCoreBase : public IA2A03 {
public:
    NESStandard system = NESStandard::NTSC;
    double speed = 1.0;
    bool paused = false;

    explicit NESCoreBase(IEmulatorHost& host, AudioSettings& audioSettings)
        : a2a03(*this, audioSettings), host(host) {
        cpuRam.fill(0x00);
    }
    virtual ~NESCoreBase() = default;

    double getCPUClockRate() const {
        return NES::CPU_CLOCK_NTSC;
    }

    double getBaseFramerate() const {
        return NES::REFRESH_RATE_NTSC_ON; // dla włączonego renderingu w PPU
    }

    double getTargetFramerate() const {
        return getBaseFramerate() * speed;
    }

    void tickFrame() {
        if (paused) return;
        int frames = getCompletedFramesCount();
        do { clockOneCycle(); } while (getCompletedFramesCount() == frames);
    }

    template <typename ConditionFunc>
    void tickWhile(ConditionFunc condition) {
        if (paused) return;
        while (condition()) clockOneCycle();
    }

    virtual void reset() {}
    virtual int getCompletedFramesCount() = 0;
    virtual uint32_t* getFramebuffer() = 0;

    int getCurrentScanline() {
        PPU2C02* p = getPPU();
        return p ? p->getScanline() : -1;
    }

    uint8_t a2a03ReadData(uint16_t addr) override { return memRead(addr); }
    uint8_t a2a03ReadDataExternal(uint16_t addr) override { return memReadExternal(addr); }
    void    a2a03WriteData(uint16_t addr, uint8_t data) override { memWrite(addr, data); }

protected:
    virtual uint8_t memRead(uint16_t addr) = 0;
    virtual uint8_t memReadExternal(uint16_t addr) { return memRead(addr); }
    virtual void    memWrite(uint16_t addr, uint8_t data) = 0;
    virtual void    clockMapper() {}
    virtual float   mapperAudio() const { return 0.0f; }

    virtual void onPreStep() {}
    virtual void onPostStep() {}
    virtual void onPpuStep(int subIdx) { (void)subIdx; }

    A2A03 a2a03;
    std::array<uint8_t, 2048> cpuRam;

private:
    IEmulatorHost& host;

    virtual PPU2C02* getPPU() { return nullptr; }

    void clockOneCycle() {
        int frames = getCompletedFramesCount();

        onPreStep();

        PPU2C02* ppu = getPPU();
        if (ppu) { ppu->clock(); onPpuStep(0); }
        if (ppu) { ppu->clock(); onPpuStep(1); }

        clockMapper();
        a2a03.clockPhi1();

        if (ppu) { ppu->clock(); onPpuStep(2); }

        a2a03.clockPhi2();

        onPostStep();

        double dt = 1.0 / (getCPUClockRate() * speed);
        host.pushAudioSample(a2a03.getAPU().getOutputSample() + mapperAudio(), dt);

        if (getCompletedFramesCount() != frames) {
            host.onFrameReady();
        }
    }
};