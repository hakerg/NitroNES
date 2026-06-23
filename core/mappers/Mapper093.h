#pragma once
#include "MapperBase.h"
#include "MapperRegistry.h"

// ----------------------------------------------------------------------------
// Mapper 093 - Sunsoft-2 (Fantasy Zone JP)
// ----------------------------------------------------------------------------
// PRG: 16 KB switchable @ $8000, {-1} fixed @ $C000.
// CHR-RAM: 8 KB (brak swappingu CHR-ROM, bit M tu jest "CHR enable" -
// ignorowany). $8000-FFFF: [PPPP ...M]  P = PRG bank (bity 4..6 wedlug
// nes_specs - uzyjemy data>>4). Mirroring: hardwired (z naglowka). Doc Disch
// mowi "M = Mirroring" - ale to bledne tradycyjnie; faktycznie Sunsoft-2 uzywa
// stalego mirroringu. Trzymamy sie zachowania zgodnego z testowymi ROMami: PRG
// bank = (data >> 4) & 0x07.
// ----------------------------------------------------------------------------
class Mapper093 : public Mapper {
public:
    using Mapper::Mapper;

    void reset() override { prg = 0; }

    bool cpuMapRead(uint16_t addr, uint32_t &mapped, uint8_t &) override {
        if (addr < 0x8000)
            return false;
        mapped = mapper_helpers::mapPrg16k_fixedHi(addr, prg, prgBanks);
        return true;
    }
    void cpuMapWrite(uint16_t addr, uint32_t &, uint8_t data) override {
        if (addr < 0x8000)
            return;
        prg = (data >> 4) & 0x0F;
    }
    bool ppuMapRead(uint16_t addr, uint32_t &mapped) override {
        if (addr > 0x1FFF)
            return false;
        mapped = mapper_helpers::mapChr8k(addr, 0, chrBanks);
        return true;
    }
    bool ppuMapWrite(uint16_t addr, uint32_t &mapped) override {
        return mapper_helpers::chrRamWrite(addr, mapped, chrBanks);
    }
    bool hasBusConflicts() const override { return true; }

private:
    uint8_t prg = 0;
};

REGISTER_MAPPER(93, Mapper093)
