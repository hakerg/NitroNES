#pragma once
#include "MapperBase.h"
#include "MapperRegistry.h"

// ----------------------------------------------------------------------------
// Mapper 011 - Color Dreams
// ----------------------------------------------------------------------------
class Mapper011 : public Mapper {
public:
    using Mapper::Mapper;
    void reset() override {
        prgBankSelect = (prgBanks / 2) > 0 ? (prgBanks / 2 - 1) & 0x03 : 0;
        chrBankSelect = 0;
    }

    bool cpuMapRead(uint16_t addr, uint32_t &mapped, uint8_t &) override {
        if (addr >= 0x8000) {
            mapped = mapper_helpers::mapPrg32k(addr, prgBankSelect, prgBanks);
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
            mapped = (uint32_t)chrBank() * 0x2000 + addr;
            return true;
        }
        return false;
    }
    bool ppuMapWrite(uint16_t addr, uint32_t &mapped) override {
        if (chrBanks > 0 || addr > 0x1FFF)
            return false;
        mapped = (uint32_t)chrBank() * 0x2000 + addr;
        return true;
    }

    bool hasBusConflicts() const override { return true; }

private:
    uint16_t chrBank() const {
        uint16_t banks = chrBanks > 0 ? chrBanks : chrRam1kBanks / 8;
        return mapper_helpers::maskBank(chrBankSelect, banks);
    }
    uint8_t prgBankSelect = 0;
    uint8_t chrBankSelect = 0;
    const char* name() const override { return "Color Dreams"; }
};

REGISTER_MAPPER(11, Mapper011)
