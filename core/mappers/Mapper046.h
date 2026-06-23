#pragma once
#include "MapperBase.h"
#include "MapperRegistry.h"

// ----------------------------------------------------------------------------
// Mapper 046 - Rumblestation 15-in-1
// ----------------------------------------------------------------------------
// $6000-7FFF: [CCCC PPPP]  high CHR bits, high PRG bits
// $8000-FFFF: [.CCC ...P]  low CHR bit, low PRG bit
//   C selektuje 8 KB CHR @ $0000 (7-bit total: hi<<3 | lo)
//   P selektuje 32 KB PRG @ $8000 (5-bit total: hi<<1 | lo)
// Powerup: $6000 = 0.
// ----------------------------------------------------------------------------
class Mapper046 : public Mapper {
public:
    using Mapper::Mapper;

    void reset() override { prgHi = chrHi = prgLo = chrLo = 0; }

    bool cpuMapRead(uint16_t addr, uint32_t &mapped, uint8_t &) override {
        if (addr < 0x8000)
            return false;
        uint8_t prg = (uint8_t)((prgHi << 1) | prgLo);
        mapped = mapper_helpers::mapPrg32k(addr, prg, prgBanks);
        return true;
    }
    void cpuMapWrite(uint16_t addr, uint32_t &, uint8_t data) override {
        if (addr >= 0x6000 && addr < 0x8000) {
            prgHi = data & 0x0F;
            chrHi = (data >> 4) & 0x0F;
        } else if (addr >= 0x8000) {
            prgLo = data & 0x01;
            chrLo = (data >> 4) & 0x07;
        }
    }
    bool ppuMapRead(uint16_t addr, uint32_t &mapped) override {
        if (addr > 0x1FFF)
            return false;
        uint8_t chr = (uint8_t)((chrHi << 3) | chrLo);
        mapped = mapper_helpers::mapChr8k(addr, chr, chrBanks);
        return true;
    }
    bool ppuMapWrite(uint16_t addr, uint32_t &mapped) override {
        return mapper_helpers::chrRamWrite(addr, mapped, chrBanks);
    }

private:
    uint8_t prgHi = 0, chrHi = 0, prgLo = 0, chrLo = 0;
};

REGISTER_MAPPER(46, Mapper046)
