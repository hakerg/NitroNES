#pragma once
#include "MapperBase.h"
#include "MapperRegistry.h"

// Mapper 097 - Irem. 16k PRG @ $C000 (zamiast $8000). Dwa bity mirroringu.
class Mapper097 : public Mapper {
public:
    using Mapper::Mapper;
    void reset() override {
        prg = 0;
        mirrorMode = Mirroring::ONESCREEN_LO;
    }
    bool cpuMapRead(uint16_t a, uint32_t &mapped, uint8_t &) override {
        if (a < 0x8000)
            return false;
        if (a < 0xC000)
            mapped = (uint32_t)(prgBanks - 1) * 0x4000 + (a & 0x3FFF);
        else
            mapped = (uint32_t)prg * 0x4000 + (a & 0x3FFF);
        return true;
    }
    void cpuMapWrite(uint16_t a, uint32_t &, uint8_t data) override {
        if (a < 0x8000)
            return;
        prg = data & 0x0F;
        switch ((data >> 6) & 0x03) {
        case 0:
            mirrorMode = Mirroring::ONESCREEN_LO;
            break;
        case 1:
            mirrorMode = Mirroring::HORIZONTAL;
            break;
        case 2:
            mirrorMode = Mirroring::VERTICAL;
            break;
        case 3:
            mirrorMode = Mirroring::ONESCREEN_HI;
            break;
        }
    }
    bool ppuMapRead(uint16_t a, uint32_t &mapped) override {
        if (a > 0x1FFF)
            return false;
        mapped = a;
        return true;
    }
    bool ppuMapWrite(uint16_t a, uint32_t &mapped) override {
        return mapper_helpers::chrRamWrite(a, mapped, chrBanks);
    }
    Mirroring mirror() const override { return mirrorMode; }
    bool hasDynamicMirror() const override { return true; }

private:
    uint8_t prg = 0;
    Mirroring mirrorMode = Mirroring::ONESCREEN_LO;
public:
    std::unique_ptr<Mapper> clone() const override { return std::make_unique<Mapper097>(*this); }
    const char* name() const override { return "Irem. 16k PRG @ $C000 (zamiast $8000). Dwa bity mirroringu."; }
};

REGISTER_MAPPER(97, Mapper097)
