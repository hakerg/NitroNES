#pragma once
#include "MapperBase.h"
#include "MapperRegistry.h"

// ----------------------------------------------------------------------------
// Mapper 000 - NROM
// ----------------------------------------------------------------------------
class Mapper000 : public Mapper {
public:
    using Mapper::Mapper;

    bool cpuMapRead(uint16_t addr, uint32_t &mapped, uint8_t &) override {
        if (addr >= 0x8000) {
            mapped = addr & (prgBanks > 1 ? 0x7FFF : 0x3FFF);
            return true;
        }
        return false;
    }
    void cpuMapWrite(uint16_t, uint32_t &, uint8_t) override { return; }

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
    const char* name() const override { return "NROM"; }
};

REGISTER_MAPPER(0, Mapper000)
