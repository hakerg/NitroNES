#pragma once

class Cartridge;
class PPU2C02;
class APU;
class IA2A03;
class ICPUBus;
class IDMA;
struct AudioSettings;

class NESBus {
public:
    static NESBus& instance() {
        static NESBus bus;
        return bus;
    }

    Cartridge* cart = nullptr;
    PPU2C02* ppu = nullptr;
    APU* apu = nullptr;
    IA2A03* core = nullptr;
    ICPUBus* cpuBus = nullptr;
    IDMA* dmaBus = nullptr;
    AudioSettings* audio = nullptr;
    bool useBackdropForBackground = false;
    bool preserveAspectRatio = true;
    uint32_t backdropPixel = 0xFF000000;

private:
    NESBus() = default;
};
