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

class NESCoreBase : public IFrameConsumer, public ICPUBus {
public:
    bool    pal = false;
    double  speed = 1.0;
    bool    paused = false;

    explicit NESCoreBase(IEmulatorHost& host)
        : a2a03(*this), host(host) {
        cpuRam.fill(0x00);
    }
    virtual ~NESCoreBase() = default;

    double getCPUClockRate() const {
        return pal ? NES::CPU_CLOCK_PAL : NES::CPU_CLOCK_NTSC;
    }

    void tickFrame(double& outDT) {
        if (paused) { outDT = 0.1; return; }
        pendingDT  = 0.0;
        frameReady = false;
        do {
            clockOneCycle();
        } while (!frameReady);
        outDT = pendingDT;
    }

    template <typename ConditionFunc>
    void tickWhile(ConditionFunc condition) {
        if (paused) return;
        while (condition()) {
            clockOneCycle();
        }
    }

    virtual void reset() {}
    virtual void shutdown() {}
    virtual void renderFrame() {}

    PPU2C02* getPPU() { return const_cast<PPU2C02*>(static_cast<const NESCoreBase*>(this)->getPPU()); }
    virtual const PPU2C02* getPPU() const { return nullptr; }
    bool hasPPU() const { return getPPU() != nullptr; }
    int  getCurrentScanline() const {
        const PPU2C02* p = getPPU();
        return p ? p->getScanline() : -1;
    }

    void onFrameComplete() override {
        frameReady = true;
        host.onFrameReady();
    }

    uint8_t cpuRead(uint16_t addr) override {
        return memRead(addr);
    }

    void cpuWrite(uint16_t addr, uint8_t data) override {
        memWrite(addr, data);
    }

    void cpuIrqAck() override { mapperIrqAck(); }

protected:
    virtual uint8_t memRead(uint16_t addr) = 0;
    virtual void    memWrite(uint16_t addr, uint8_t data) = 0;
    virtual void    clockMapper() {}
    virtual float   mapperAudio() const { return 0.0f; }
    virtual bool    mapperIRQ() const { return false; }
    virtual void    mapperIrqAck() {}

    virtual void onPreStep() {}

    A2A03 a2a03;
    std::array<uint8_t, 2048> cpuRam;

private:
    IEmulatorHost& host;
    bool   frameReady    = false;
    double pendingDT     = 0.0;

    void clockOneCycle() {
        onPreStep();

        PPU2C02* ppu = getPPU();
        if (ppu) { ppu->clock(); ppu->clock(); }

        clockMapper();
        a2a03.clockPhi1();

        if (ppu) ppu->clock();

        a2a03.clockPhi2();

        double dt = 1.0 / (getCPUClockRate() * speed);
        pendingDT += dt;
        host.pushAudioSample(a2a03.getAPU().getOutputSample() + mapperAudio(), dt);
    }
};