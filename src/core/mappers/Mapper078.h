#pragma once
#include "MapperBase.h"
#include "MapperRegistry.h"

// Mapper 078 - Cosmo Carrier / Holy Diver. Bus conflicts.
// Mirroring jest jednoekranowy lub H/V w zale�no�ci od gry � wybieramy
// uproszczone H/V (dzia�a dla Holy Divera; dla Cosmo Carriera mo�na r�cznie
// podmieni� mapper na wariant 1ScA/1ScB).
class Mapper078 : public Mapper {
public:
    using Mapper::Mapper;
    void reset() override {
        prg = (prgBanks / 2) > 0 ? (prgBanks / 2 - 1) & 0x07 : 0;
        chr = 0;
        mirrorMode = Mirroring::HORIZONTAL;
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
        prg = data & 0x07;
        mirrorMode =
            (data & 0x08) ? Mirroring::VERTICAL : Mirroring::HORIZONTAL;
        chr = (data >> 4) & 0x0F;
    }
    bool ppuMapRead(uint16_t a, uint32_t &mapped) override {
        if (a > 0x1FFF)
            return false;
        mapped = (uint32_t)mapper_helpers::maskBank(chr, chrBanks) * 0x2000 + (a & 0x1FFF);
        return true;
    }
    bool ppuMapWrite(uint16_t a, uint32_t &mapped) override {
        return mapper_helpers::chrRamWrite(a, mapped, chrBanks);
    }
    Mirroring mirror() const override { return mirrorMode; }
    bool hasDynamicMirror() const override { return true; }

private:
    uint8_t prg = 0, chr = 0;
    Mirroring mirrorMode = Mirroring::HORIZONTAL;
    const char* name() const override { return "Cosmo Carrier / Holy Diver. Bus conflicts."; }
};

REGISTER_MAPPER(78, Mapper078)
