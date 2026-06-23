#pragma once
#include "MapperBase.h"
#include "MapperRegistry.h"

// ----------------------------------------------------------------------------
// Mapper 231 - 20-in-1 multicart
// ----------------------------------------------------------------------------
// $8000-FFFF: A~[.... .... M.LP PPP.]
//   M = mirror (0=V, 1=H), L = low bit PRG, P = high bits PRG.
// $8000-BFFF: bank AND $1E (zerowy low bit -> wyrownanie do 32K),
// $C000-FFFF: pelen bank.
// CHR-RAM 8 KB.
// ----------------------------------------------------------------------------
class Mapper231 : public Mapper {
public:
    using Mapper::Mapper;

    void reset() override {
        reg = 0;
        mirrorMode = Mirroring::VERTICAL;
    }

    bool cpuMapRead(uint16_t addr, uint32_t &mapped, uint8_t &) override {
        if (addr < 0x8000)
            return false;
        uint8_t bank = (addr < 0xC000) ? (uint8_t)(reg & 0x1E) : reg;
        bank = mapper_helpers::maskBank(bank, prgBanks);
        mapped = (uint32_t)bank * 0x4000 + (addr & 0x3FFF);
        return true;
    }

    void cpuMapWrite(uint16_t addr, uint32_t &, uint8_t /*data*/) override {
        if (addr < 0x8000)
            return;
        // addr layout: M=bit7, L=bit5, P=bits4..1
        uint16_t a = addr;
        uint8_t L = (uint8_t)((a >> 5) & 0x01);
        uint8_t P = (uint8_t)((a >> 1) & 0x0F);
        reg = (uint8_t)((P << 1) | L);
        mirrorMode = (a & 0x80) ? Mirroring::HORIZONTAL : Mirroring::VERTICAL;
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
    uint8_t reg = 0;
    Mirroring mirrorMode = Mirroring::VERTICAL;
};

REGISTER_MAPPER(231, Mapper231)
