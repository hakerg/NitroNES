#pragma once
#include <functional>
#include "../src/core/NESSystem.h"

class NESHeadlessSystem
    : public IEmulatorHost
    , public INESSystemHost
    , public NESSystem
{
public:
    explicit NESHeadlessSystem(const std::string& path)
        : NESSystem(
            static_cast<IEmulatorHost&>(*this),
            static_cast<INESSystemHost&>(*this),
            audioSettings,
            path) {}

    void pushAudioSample(float, double) override {}
    void onFrameReady() override { ++frameCounter; }
    void renderFrame(const uint32_t* fb) override { lastFrame = fb; }

    uint8_t readController(int port) override {
        return port == 0 ? controller1 : controller2;
    }

    void setController1(uint8_t buttons) { controller1 = buttons; }
    void setController2(uint8_t buttons) { controller2 = buttons; }
    uint8_t getController1() const { return controller1; }
    uint8_t getController2() const { return controller2; }

    void tickCycles(uint64_t n) {
        uint64_t target = cpuCycle + n;
        tickWhile([&]{ return cpuCycle < target; });
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

    const uint32_t* framebuffer() const { return lastFrame; }

    uint8_t peekCPU(uint16_t addr) {
        if (addr < 0x2000) return cpuRam[addr & 0x07FF];
        if (cart && addr >= 0x4020) return cart->cpuRead(addr, 0x00);
        return 0xFF;
    }
    uint8_t   peekRAM(uint16_t addr) { return cpuRam[addr & 0x07FF]; }
    uint16_t  cpuPC() { return a2a03.getCPU().PC; }
    uint64_t  frameNo() const { return frameCounter; }
    uint64_t  cycleNo() const { return cpuCycle; }

    A2A03&   getA2A03()  { return a2a03; }
    PPU2C02& getPPURef() { return ppu;   }

    std::function<void()> preStepHook;
    std::function<void()> postStepHook;
    std::function<void(int)> ppuStepHook;

protected:
    void onPreStep() override {
        NESSystem::onPreStep();
        ++cpuCycle;
        if (preStepHook) preStepHook();
    }

    void onPostStep() override {
        NESSystem::onPostStep();
        if (postStepHook) postStepHook();
    }

    void onPpuStep(int subIdx) override {
        if (ppuStepHook) ppuStepHook(subIdx);
    }

    AudioSettings audioSettings;

private:
    uint8_t controller1 = 0x00;
    uint8_t controller2 = 0x00;
    const uint32_t* lastFrame = nullptr;
    uint64_t cpuCycle    = 0;
    uint64_t frameCounter = 0;
};

