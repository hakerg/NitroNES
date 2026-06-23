#pragma once
#include "MapperBase.h"
#include "MapperRegistry.h"

// ----------------------------------------------------------------------------
// Mapper 233 - "42-in-1" (zgodnie z dokumentacja Disch'a)
// ----------------------------------------------------------------------------
// $8000-FFFF: [MMOP PPPP]
//   M = mirroring (00=zob. nizej, 01=V, 10=H, 11=1ScB)
//   O = PRG mode (0=32K, 1=16K duplikat)
//   P = PRG page (5 bit)
// Mode 00: kombinacja [NTA][NTA]/[NTA][NTB] - przybliz przez ONESCREEN_LO.
// ----------------------------------------------------------------------------
class Mapper233 : public Mapper {
public:
    using Mapper::Mapper;

    void reset() override { reg = 0; }

    bool cpuMapRead(uint16_t addr, uint32_t &mapped, uint8_t &) override {
        if (addr < 0x8000)
            return false;
        uint8_t p = reg & 0x1F;
        bool mode32 = (reg & 0x20) == 0;
        if (mode32) {
            uint8_t bank32 = (uint8_t)(p >> 1);
            mapped = mapper_helpers::mapPrg32k(addr, bank32, prgBanks);
        } else {
            uint32_t off =
                (uint32_t)mapper_helpers::maskBank(p, prgBanks) * 0x4000;
            mapped = off + (addr & 0x3FFF);
        }
        return true;
    }
    void cpuMapWrite(uint16_t addr, uint32_t &, uint8_t data) override {
        if (addr < 0x8000)
            return;
        reg = data;
    }
    bool ppuMapRead(uint16_t addr, uint32_t &mapped) override {
        if (addr > 0x1FFF)
            return false;
        mapped = mapper_helpers::mapChr8k(addr, 0, chrBanks);
        return true;
    }
    bool ppuMapWrite(uint16_t addr, uint32_t &mapped) override {
        return mapper_helpers::chrRamWrite(addr, mapped, chrBanks);
    }

    Mirroring mirror() const override {
        switch ((reg >> 6) & 0x03) {
        case 0:
            return Mirroring::ONESCREEN_LO; // dirty approximation
        case 1:
            return Mirroring::VERTICAL;
        case 2:
            return Mirroring::HORIZONTAL;
        default:
            return Mirroring::ONESCREEN_HI;
        }
    }
    bool hasDynamicMirror() const override { return true; }

private:
    uint8_t reg = 0;
};

REGISTER_MAPPER(233, Mapper233)
