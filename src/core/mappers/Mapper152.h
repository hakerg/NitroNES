#pragma once
#include "MapperBase.h"
#include "MapperRegistry.h"

// Mapper 152 - Bandai (warianty: Arkanoid 2, Gegege no Kitarou 2). Bus
// conflicts.
class Mapper152 : public Mapper {
public:
    using Mapper::Mapper;
    void reset() override {
        prg = 0;
        chr = 0;
        mirrorMode = Mirroring::ONESCREEN_LO;
    }
    bool hasBusConflicts() const override { return true; }
    bool cpuMapRead(uint16_t a, uint32_t &mapped, uint8_t &) override {
        if (a < 0x8000)
            return false;
        if (a < 0xC000)
            mapped = (uint32_t)prg * 0x4000 + (a & 0x3FFF);
        else
            mapped = (uint32_t)(prgBanks - 1) * 0x4000 + (a & 0x3FFF);
        return true;
    }
    void cpuMapWrite(uint16_t a, uint32_t &, uint8_t data) override {
        if (a < 0x8000)
            return;
        prg = (data >> 4) & 0x07;
        chr = data & 0x0F;
        mirrorMode =
            (data & 0x80) ? Mirroring::ONESCREEN_HI : Mirroring::ONESCREEN_LO;
    }
    bool ppuMapRead(uint16_t a, uint32_t &mapped) override {
        if (a > 0x1FFF)
            return false;
        mapped = (uint32_t)chr * 0x2000 + (a & 0x1FFF);
        return true;
    }
    bool ppuMapWrite(uint16_t a, uint32_t &mapped) override {
        return mapper_helpers::chrRamWrite(a, mapped, chrBanks);
    }
    Mirroring mirror() const override { return mirrorMode; }
    bool hasDynamicMirror() const override { return true; }

private:
    uint8_t prg = 0, chr = 0;
    Mirroring mirrorMode = Mirroring::ONESCREEN_LO;
    const char* name() const override { return "Bandai (warianty: Arkanoid 2, Gegege no Kitarou 2). Bus"; }
};

REGISTER_MAPPER(152, Mapper152)
