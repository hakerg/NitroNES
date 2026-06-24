#pragma once
#include "MapperBase.h"
#include "MapperRegistry.h"

// ----------------------------------------------------------------------------
// Mapper 007 - AxROM
// ----------------------------------------------------------------------------
class Mapper007 : public Mapper {
public:
    using Mapper::Mapper;

    void reset() override {
        prgBankSelect = 0;
        mirrorMode = Mirroring::ONESCREEN_LO;
    }

    bool cpuMapRead(uint16_t addr, uint32_t &mapped, uint8_t &) override {
        if (addr >= 0x8000) {
            mapped = mapper_helpers::mapPrg32k(addr, prgBankSelect, prgBanks);
            return true;
        }
        return false;
    }
    void cpuMapWrite(uint16_t addr, uint32_t &, uint8_t data) override {
        if (addr >= 0x8000) {
            prgBankSelect = data & 0x07;
            mirrorMode = (data & 0x10) ? Mirroring::ONESCREEN_HI
                                       : Mirroring::ONESCREEN_LO;
        }
    }
    bool ppuMapRead(uint16_t addr, uint32_t &mapped) override {
        if (addr <= 0x1FFF) {
            mapped = addr;
            return true;
        }
        return false;
    }
    bool ppuMapWrite(uint16_t addr, uint32_t &mapped) override {
        return mapper_helpers::chrRamWrite(addr, mapped, chrBanks);
    }
    Mirroring mirror() const override { return mirrorMode; }
    bool hasDynamicMirror() const override { return true; }
    // AxROM (a zwlaszcza AOROM uzywany przez Battletoads) nie ma bus conflicts.
    // ANROM ma, ale gry ANROM zapisuja "bezpieczne" wartosci, wiec false jest
    // OK dla calej rodziny. Wlaczone konflikty rozwalaja Battletoads (zly bank
    // PRG po wyborze levelu -> czarny ekran, glitch statku w intro).
    bool hasBusConflicts() const override { return false; }

private:
    uint8_t prgBankSelect = 0;
    Mirroring mirrorMode = Mirroring::ONESCREEN_LO;
};

REGISTER_MAPPER(7, Mapper007)
