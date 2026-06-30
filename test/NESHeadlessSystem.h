#pragma once
#include "../src/core/NESSystem.h"

class NESHeadlessSystem
    : public NESSystem
{
public:
    explicit NESHeadlessSystem(const std::string& path)
        : NESSystem(
            audioSettings,
            path) {}

    void setController1(uint8_t buttons) { controller1 = buttons; }
    void setController2(uint8_t buttons) { controller2 = buttons; }
    uint8_t getController1() const { return controller1; }
    uint8_t getController2() const { return controller2; }

    void tickCycles(uint64_t n) {
        const uint64_t target = cycleNo() + n;
        tickWhile([&]{ return cycleNo() < target; });
    }

    void dumpNametable(uint8_t dst[960], int nt = 0) {
        uint16_t base = (nt == 0) ? 0x2000u : 0x2400u;
        for (int i = 0; i < 960; ++i)
            dst[i] = ppu.ppuRead(base + static_cast<uint16_t>(i));
    }

    void dumpOAM(uint8_t dst[256]) {
        const uint8_t* oam = ppu.oamData();
        for (int i = 0; i < 256; ++i) dst[i] = oam[i];
    }

    uint32_t* framebuffer() { return ppu.getFramebuffer(); }

    uint8_t peekCPU(uint16_t addr) {
        if (addr < 0x2000) return cpuRam[addr & 0x07FF];
        if (addr >= 0x4020) return cart.cpuRead(addr, 0x00);
        return 0xFF;
    }
    uint64_t  frameNo() { return (uint64_t)ppu.getCompletedFramesCount(); }
    uint64_t  cycleNo() { return a2a03.getCPU().getCycle(); }

protected:
    uint8_t readController(int port) override {
        return port == 0 ? controller1 : controller2;
    }

    void pushAudioSample(float sample, double dt) override {}

    AudioSettings audioSettings;

private:
    uint8_t controller1 = 0x00;
    uint8_t controller2 = 0x00;
};
