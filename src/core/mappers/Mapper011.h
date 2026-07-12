#pragma once
#include "MapperBase.h"
#include "MapperRegistry.h"

// ----------------------------------------------------------------------------
// Mapper 011 - Color Dreams
// ----------------------------------------------------------------------------
class Mapper011 : public Mapper {
public:
    using Mapper::Mapper;

    bool cpuMapRead(uint16_t addr, uint32_t &mapped, uint8_t &) override {
        if (addr >= 0x8000) {
            mapped = (uint32_t)prgBankSelect * 0x8000 + (addr & 0x7FFF);
            return true;
        }
        return false;
    }
    void cpuMapWrite(uint16_t addr, uint32_t &, uint8_t data) override {
        if (addr >= 0x8000) {
            prgBankSelect = data & 0x03;
            chrBankSelect = (data >> 4) & 0x0F;
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
    uint8_t prgBankSelect = 0;
    uint8_t chrBankSelect = 0;
    const char* name() const override { return "Color Dreams"; }
};

REGISTER_MAPPER(11, Mapper011)
