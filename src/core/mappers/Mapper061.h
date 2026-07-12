#pragma once
#include "MapperBase.h"
#include "MapperRegistry.h"

// ----------------------------------------------------------------------------
// Mapper 061 - 20-in-1
// ----------------------------------------------------------------------------
// $8000-FFFF: A~[.... .... M.LO HHHH]
//   H = high 4 bits of PRG reg, L = low bit, O = PRG mode, M = mirror (0=V,1=H)
// Mode 0: 32K bank pod $8000-FFFF; Mode 1: 16K mirror.
// CHR: 8K CHR-RAM (zwykle).
// ----------------------------------------------------------------------------
class Mapper061 : public Mapper {
public:
    using Mapper::Mapper;

    void reset() override { addrLatch = 0; }

    bool cpuMapRead(uint16_t addr, uint32_t &mapped, uint8_t &) override {
        if (addr < 0x8000)
            return false;
        uint8_t H = (uint8_t)(addrLatch & 0x0F);
        uint8_t L = (uint8_t)((addrLatch >> 5) & 0x01);
        bool mode32 = ((addrLatch >> 4) & 0x01) == 0;
        uint8_t prgReg = (uint8_t)((H << 1) | L);
        if (mode32) {
            uint8_t bank = (uint8_t)(prgReg >> 1);
            mapped = mapper_helpers::mapPrg32k(addr, bank, prgBanks);
        } else {
            // 16K mirror
            uint32_t off =
                (uint32_t)mapper_helpers::maskBank(prgReg, prgBanks) * 0x4000;
            mapped = off + (addr & 0x3FFF);
        }
        return true;
    }
    void cpuMapWrite(uint16_t addr, uint32_t &, uint8_t) override {
        if (addr < 0x8000)
            return;
        addrLatch = addr;
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
        return (addrLatch & 0x80) ? Mirroring::HORIZONTAL : Mirroring::VERTICAL;
    }
    bool hasDynamicMirror() const override { return true; }

private:
    uint16_t addrLatch = 0;
    const char* name() const override { return "20-in-1"; }
};

REGISTER_MAPPER(61, Mapper061)
