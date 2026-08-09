#pragma once
#include "MapperBase.h"
#include "MapperRegistry.h"

// ----------------------------------------------------------------------------
// Mapper 092 - Jaleco JF-19 (Moero!! Pro Yakyuu '88, Moero!! Pro Soccer)
// ----------------------------------------------------------------------------
// Identyczny jak mapper 072 ale PRG inaczej:
//   $8000  fixed = 0
//   $C000  switchable (PRG reg)
// Audio expansion - ignorujemy.
// ----------------------------------------------------------------------------
class Mapper092 : public Mapper {
public:
    using Mapper::Mapper;

    void reset() override {
        prgBank = 0;
        chrBank = 0;
        lastP = false;
        lastC = false;
    }

    bool cpuMapRead(uint16_t addr, uint32_t &mapped, uint8_t &) override {
        if (addr < 0x8000)
            return false;
        mapped = mapper_helpers::mapPrg16k_fixedLo(addr, prgBank, prgBanks);
        return true;
    }
    void cpuMapWrite(uint16_t addr, uint32_t &, uint8_t data) override {
        if (addr < 0x8000)
            return;
        bool p = (data & 0x80) != 0;
        bool c = (data & 0x40) != 0;
        if (p && !lastP)
            prgBank = data & 0x0F;
        if (c && !lastC)
            chrBank = data & 0x0F;
        lastP = p;
        lastC = c;
    }
    bool ppuMapRead(uint16_t addr, uint32_t &mapped) override {
        if (addr > 0x1FFF)
            return false;
        mapped = mapper_helpers::mapChr8k(addr, chrBank, chrBanks);
        return true;
    }
    bool ppuMapWrite(uint16_t addr, uint32_t &mapped) override {
        return mapper_helpers::chrRamWrite(addr, mapped, chrBanks);
    }
    bool hasBusConflicts() const override { return true; }

private:
    uint8_t prgBank = 0, chrBank = 0;
    bool lastP = false, lastC = false;
public:
    std::unique_ptr<Mapper> clone() const override { return std::make_unique<Mapper092>(*this); }
    const char* name() const override { return "Jaleco JF-19 (Moero!! Pro Yakyuu '88, Moero!! Pro Soccer)"; }
};

REGISTER_MAPPER(92, Mapper092)
