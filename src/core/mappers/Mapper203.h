#pragma once
#include "MapperBase.h"
#include "MapperRegistry.h"

// ----------------------------------------------------------------------------
// Mapper 203 - 35-in-1 multicart
// ----------------------------------------------------------------------------
// $8000-FFFF: [PPPP PPCC]  P=PRG bank (16K, mirrored), C=CHR bank (8K).
// ----------------------------------------------------------------------------
class Mapper203 : public Mapper {
public:
    using Mapper::Mapper;

    void reset() override {
        prg = 0;
        chr = 0;
    }

    bool cpuMapRead(uint16_t addr, uint32_t &mapped, uint8_t &) override {
        if (addr < 0x8000)
            return false;
        uint8_t b = mapper_helpers::maskBank(prg, prgBanks);
        mapped = (uint32_t)b * 0x4000 + (addr & 0x3FFF);
        return true;
    }
    void cpuMapWrite(uint16_t addr, uint32_t &, uint8_t data) override {
        if (addr < 0x8000)
            return;
        prg = (uint8_t)((data >> 2) & 0x3F);
        chr = (uint8_t)(data & 0x03);
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
public:
    std::unique_ptr<Mapper> clone() const override { return std::make_unique<Mapper203>(*this); }
    const char* name() const override { return "35-in-1 multicart"; }
};

REGISTER_MAPPER(203, Mapper203)
