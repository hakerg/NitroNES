#pragma once
#include "MapperBase.h"
#include "MapperRegistry.h"

// ----------------------------------------------------------------------------
// Mapper 003 - CNROM
// ----------------------------------------------------------------------------
class Mapper003 : public Mapper {
public:
    using Mapper::Mapper;

    bool cpuMapRead(uint16_t addr, uint32_t &mapped, uint8_t &) override {
        if (addr >= 0x8000) {
            mapped = addr & (prgBanks > 1 ? 0x7FFF : 0x3FFF);
            return true;
        }
        return false;
    }
    void cpuMapWrite(uint16_t addr, uint32_t &, uint8_t data) override {
        if (addr >= 0x8000) {
            chrBankSelect = data & 0x03;
            return;
        }
    }
    bool ppuMapRead(uint16_t addr, uint32_t &mapped) override {
        if (addr <= 0x1FFF) {
            mapped = (uint32_t)chrBankSelect * 0x2000 + addr;
            return true;
        }
        return false;
    }
    bool ppuMapWrite(uint16_t, uint32_t &) override { return false; }

    bool hasBusConflicts() const override { return true; }

private:
    uint8_t chrBankSelect = 0;
    const char* name() const override { return "CNROM (and compatible)"; }
};

REGISTER_MAPPER(3, Mapper003)
