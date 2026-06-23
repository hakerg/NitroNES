#pragma once
#include "MapperBase.h"
#include "MapperRegistry.h"

// ----------------------------------------------------------------------------
// Mapper 107 - Magic Dragon
// ----------------------------------------------------------------------------
// $8000-FFFF: [PPPP PPP.] / [CCCC CCCC] (ten sam zapis dla obu)
//   P = 32K PRG @ $8000 (data >> 1)
//   C = 8K  CHR @ $0000 (data)
// ----------------------------------------------------------------------------
class Mapper107 : public Mapper {
public:
    using Mapper::Mapper;

    void reset() override {
        prg = 0;
        chr = 0;
    }

    bool cpuMapRead(uint16_t addr, uint32_t &mapped, uint8_t &) override {
        if (addr < 0x8000)
            return false;
        mapped = mapper_helpers::mapPrg32k(addr, prg, prgBanks);
        return true;
    }
    void cpuMapWrite(uint16_t addr, uint32_t &, uint8_t data) override {
        if (addr < 0x8000)
            return;
        prg = (uint8_t)(data >> 1);
        chr = data;
    }
    bool ppuMapRead(uint16_t addr, uint32_t &mapped) override {
        if (addr > 0x1FFF)
            return false;
        mapped = mapper_helpers::mapChr8k(addr, chr, chrBanks);
        return true;
    }
    bool ppuMapWrite(uint16_t addr, uint32_t &mapped) override {
        return mapper_helpers::chrRamWrite(addr, mapped, chrBanks);
    }

private:
    uint8_t prg = 0, chr = 0;
};

REGISTER_MAPPER(107, Mapper107)
