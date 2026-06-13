#pragma once
#include "../core/NESSystem.h"

class HeadlessEmulatorHost : public IEmulatorHost {
public:
    void pushAudioSample(float, double) override {}
    void onFrameReady() override {}
};

class HeadlessNESHost : public INESSystemHost {
public:
    void renderFrame(const uint32_t*) override {}
    uint8_t readController(int) override { return 0x00; }
};

class NESHeadlessSystem
    : private HeadlessEmulatorHost
    , private HeadlessNESHost
    , public  NESSystem
{
public:
    NESHeadlessSystem()
        : NESSystem(
            static_cast<IEmulatorHost&>(*this),
            static_cast<INESSystemHost&>(*this)) {}

    void dumpNametable(uint8_t dst[960], int nt = 0) {
        uint16_t base = (nt == 0) ? 0x2000u : 0x2400u;
        for (int i = 0; i < 960; ++i)
            dst[i] = ppu.ppuRead(base + static_cast<uint16_t>(i));
    }
};

