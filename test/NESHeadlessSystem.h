#pragma once
#include "../core/NESSystem.h"

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
            path) {}

    void pushAudioSample(float, double) override {}
    void onFrameReady() override {}
    void renderFrame(const uint32_t* fb) override { lastFrame = fb; }

    uint8_t readController(int port) override {
        return port == 0 ? controller1 : controller2;
    }

    void setController1(uint8_t buttons) { controller1 = buttons; }
    void setController2(uint8_t buttons) { controller2 = buttons; }

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

private:
    uint8_t controller1 = 0x00;
    uint8_t controller2 = 0x00;
    const uint32_t* lastFrame = nullptr;
};

