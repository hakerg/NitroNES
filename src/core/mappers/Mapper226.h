#pragma once
#include "MapperBase.h"
#include "MapperRegistry.h"

// ----------------------------------------------------------------------------
// Mapper 226 - 76-in-1 / Super 42-in-1
// ----------------------------------------------------------------------------
// $8000: [PMOP PPPP]  P=low 6 bits PRG, M=mirror (0=H,1=V), O=PRG mode
// $8001: [.... ...H]  H=high bit PRG
// Reg = P | (H<<6). Mode 0 -> 32 KB @ $8000, Mode 1 -> 16 KB powtorzony.
// CHR-RAM 8 KB.
// ----------------------------------------------------------------------------
class Mapper226 : public Mapper {
public:
    using Mapper::Mapper;

    void reset() override {
        prgLo = 0;
        prgHi = 0;
        mode32 = true;
        mirrorMode = Mirroring::HORIZONTAL;
    }

    bool cpuMapRead(uint16_t addr, uint32_t &mapped, uint8_t &) override {
        if (addr < 0x8000)
            return false;
        uint8_t reg = (uint8_t)((prgHi << 6) | (prgLo & 0x3F));
        if (mode32) {
            uint8_t n = prgBanks / 2;
            if (n == 0)
                n = 1;
            uint8_t b = mapper_helpers::maskBank(reg >> 1, n);
            mapped = (uint32_t)b * 0x8000 + (addr & 0x7FFF);
        } else {
            uint8_t b = mapper_helpers::maskBank(reg, prgBanks);
            mapped = (uint32_t)b * 0x4000 + (addr & 0x3FFF);
        }
        return true;
    }

    void cpuMapWrite(uint16_t addr, uint32_t &, uint8_t data) override {
        if (addr < 0x8000)
            return;
        if ((addr & 0x8001) == 0x8000) {
            prgLo = data & 0x1F;
            prgLo |= (uint8_t)((data >> 1) & 0x20); // bit5 -> bit5 PRG (P high)
            mirrorMode =
                (data & 0x40) ? Mirroring::VERTICAL : Mirroring::HORIZONTAL;
            mode32 = (data & 0x20) == 0; // O=0 -> 32K mode
        } else {
            prgHi = data & 0x01;
        }
    }

    bool ppuMapRead(uint16_t addr, uint32_t &mapped) override {
        if (addr > 0x1FFF)
            return false;
        mapped = addr;
        return true;
    }
    bool ppuMapWrite(uint16_t addr, uint32_t &mapped) override {
        return mapper_helpers::chrRamWrite(addr, mapped, chrBanks);
    }
    Mirroring mirror() const override { return mirrorMode; }
    bool hasDynamicMirror() const override { return true; }

private:
    uint8_t prgLo = 0, prgHi = 0;
    bool mode32 = true;
    Mirroring mirrorMode = Mirroring::HORIZONTAL;
    const char* name() const override { return "76-in-1 / Super 42-in-1"; }
};

REGISTER_MAPPER(226, Mapper226)
