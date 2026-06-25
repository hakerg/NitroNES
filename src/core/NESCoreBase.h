#pragma once
#include <array>
#include "NESConst.h"
#include "A2A03.h"
#include "PPU2C02.h"

class IEmulatorHost {
public:
    virtual ~IEmulatorHost() = default;
    virtual void pushAudioSample(float sample, double dt) = 0;
    virtual void onFrameReady() = 0;
};

class NESCoreBase : public IFrameConsumer, public IA2A03 {
public:
    bool    pal    = false;
    double  speed  = 1.0;
    bool    paused = false;

    explicit NESCoreBase(IEmulatorHost& host, AudioSettings& audioSettings)
        : a2a03(*this, audioSettings), host(host) {
        cpuRam.fill(0x00);
    }
    virtual ~NESCoreBase() = default;

    double getCPUClockRate() const {
        return pal ? NES::CPU_CLOCK_PAL : NES::CPU_CLOCK_NTSC;
    }

    double getBaseFramerate() const {
        // TODO: właściwa obsługa PAL jeśli to konieczne
        return pal ? 50.0 : NES::REFRESH_RATE_NTSC_ON;
    }

    double getTargetFramerate() const {
        return getBaseFramerate() * speed;
    }

    void tickFrame(double& outDT) {
        if (paused) { outDT = 0.1; return; }
        pendingDT  = 0.0;
        frameReady = false;
        do { clockOneCycle(); } while (!frameReady);
        outDT = pendingDT;
    }

    template <typename ConditionFunc>
    void tickWhile(ConditionFunc condition) {
        if (paused) return;
        while (condition()) clockOneCycle();
    }

    virtual void reset() {}

    virtual PPU2C02* getPPU() { return nullptr; }
    bool hasPPU() { return getPPU() != nullptr; }
    int getCurrentScanline() {
        PPU2C02* p = getPPU();
        return p ? p->getScanline() : -1;
    }

    void onFrameComplete() override {
        frameReady = true;
        host.onFrameReady();
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
    bool   frameReady = false;
    double pendingDT  = 0.0;

    void clockOneCycle() {
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
        pendingDT += dt;
        host.pushAudioSample(a2a03.getAPU().getOutputSample() + mapperAudio(), dt);
    }
};