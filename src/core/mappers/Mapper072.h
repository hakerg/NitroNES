#pragma once
#include "MapperBase.h"
#include "MapperRegistry.h"

// ----------------------------------------------------------------------------
// Mapper 072 - Jaleco JF-17 (bez expansion audio - tylko PRG/CHR)
// ----------------------------------------------------------------------------
// PRG: 16 KB switchable @ $8000 (R0), {-1} fixed @ $C000.
// CHR: 8 KB switchable.
// Latch: bit7=P (PRG-strobe), bit6=C (CHR-strobe). Bank ustawia sie tylko
// na zboczu 0->1 odpowiedniego bitu. Dolne bity to dane.
// Audio (bity R, S) - ignorujemy zgodnie z zaleceniem (brak rozszerzania APU).
// ----------------------------------------------------------------------------
class Mapper072 : public Mapper {
public:
    using Mapper::Mapper;

    void reset() override {
        prgBank = 0;
        chrBank = 0;
        lastP = false;
        lastC = false;
    }

    bool cpuMapRead(uint16_t addr, uint32_t &mapped, uint8_t &) override {
        if (addr < 0x8000)
            return false;
        mapped = mapper_helpers::mapPrg16k_fixedHi(addr, prgBank, prgBanks);
        return true;
    }
    void cpuMapWrite(uint16_t addr, uint32_t &, uint8_t data) override {
        if (addr < 0x8000)
            return;
        bool p = (data & 0x80) != 0;
        bool c = (data & 0x40) != 0;
        if (p && !lastP)
            prgBank = data & 0x0F;
        if (c && !lastC)
            chrBank = data & 0x0F;
        lastP = p;
        lastC = c;
    }
    bool ppuMapRead(uint16_t addr, uint32_t &mapped) override {
        if (addr > 0x1FFF)
            return false;
        mapped = mapper_helpers::mapChr8k(addr, chrBank, chrBanks);
        return true;
    }
    bool ppuMapWrite(uint16_t addr, uint32_t &mapped) override {
        return mapper_helpers::chrRamWrite(addr, mapped, chrBanks);
    }
    bool hasBusConflicts() const override { return true; }

private:
    uint8_t prgBank = 0, chrBank = 0;
    bool lastP = false, lastC = false;
    const char* name() const override { return "Jaleco JF-17 (bez expansion audio - tylko PRG/CHR)"; }
};

REGISTER_MAPPER(72, Mapper072)
