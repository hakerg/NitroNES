#pragma once
#include "MapperBase.h"
#include "MapperRegistry.h"

// ----------------------------------------------------------------------------
// Mapper 201 - 8-in-1 / 21-in-1 multicart
// ----------------------------------------------------------------------------
// Adres zapisu wybiera bank: A~[.... .... RRRR RRRR]
// $8000-FFFF: pojedynczy 32 KB bank; CHR 8 KB.
// Dla malych ROMow zwykle uzywa sie tylko najnizszych bitow indeksu.
// ----------------------------------------------------------------------------
class Mapper201 : public Mapper {
public:
    using Mapper::Mapper;

    void reset() override { bank = 0; }

    bool cpuMapRead(uint16_t addr, uint32_t &mapped, uint8_t &) override {
        if (addr < 0x8000)
            return false;
        mapped = mapper_helpers::mapPrg32k(addr, bank, prgBanks);
        return true;
    }
    void cpuMapWrite(uint16_t addr, uint32_t &, uint8_t /*data*/) override {
        if (addr < 0x8000)
            return;
        bank = (uint8_t)(addr & 0xFF);
    }
    bool ppuMapRead(uint16_t addr, uint32_t &mapped) override {
        if (addr > 0x1FFF)
            return false;
        mapped = mapper_helpers::mapChr8k(addr, bank, chrBanks);
        return true;
    }
    bool ppuMapWrite(uint16_t addr, uint32_t &mapped) override {
        return mapper_helpers::chrRamWrite(addr, mapped, chrBanks);
    }

private:
    uint8_t bank = 0;
};

REGISTER_MAPPER(201, Mapper201)
