#pragma once
#include "MapperBase.h"
#include "MapperRegistry.h"

// ----------------------------------------------------------------------------
// Mapper 060 - Reset-Based 4-in-1 (NROM-128 x4)
// ----------------------------------------------------------------------------
// Po kazdym soft-resecie inkrementowany jest wewnetrzny 2-bitowy licznik.
// Wybrany blok: 16 KB PRG (mirrorowane do $8000 i $C000) + 8 KB CHR.
// ----------------------------------------------------------------------------
class Mapper060 : public Mapper {
public:
    using Mapper::Mapper;

    void reset() override { block = (block + 1) & 0x03; }

    bool cpuMapRead(uint16_t addr, uint32_t &mapped, uint8_t &) override {
        if (addr < 0x8000)
            return false;
        uint32_t off =
            (uint32_t)mapper_helpers::maskBank(block, prgBanks) * 0x4000;
        mapped = off + (addr & 0x3FFF);
        return true;
    }
    void cpuMapWrite(uint16_t, uint32_t &, uint8_t) override {}
    bool ppuMapRead(uint16_t addr, uint32_t &mapped) override {
        if (addr > 0x1FFF)
            return false;
        mapped = mapper_helpers::mapChr8k(addr, block, chrBanks);
        return true;
    }
    bool ppuMapWrite(uint16_t addr, uint32_t &mapped) override {
        return mapper_helpers::chrRamWrite(addr, mapped, chrBanks);
    }

private:
    uint8_t block = 0xFF; // pierwszy reset() przejdzie na 0
};

REGISTER_MAPPER(60, Mapper060)
