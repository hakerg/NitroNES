#pragma once
#include "MapperBase.h"
#include "MapperRegistry.h"

// ----------------------------------------------------------------------------
// Mapper 240 - Jing Ke Xin Zhuan / Sheng Huo Lie Zhuan
// ----------------------------------------------------------------------------
// $4020-$5FFF: [PPPP CCCC]  P=32K PRG @ $8000, C=8K CHR @ $0000
// (Rejestry leza w obszarze $4020-$5FFF; nie ma bus conflicts).
// ----------------------------------------------------------------------------
class Mapper240 : public Mapper {
public:
    using Mapper::Mapper;

    void reset() override {
        prg = 0;
        chr = 0;
    }

    bool cpuMapRead(uint16_t addr, uint32_t &mapped, uint8_t &) override {
        if (addr < 0x8000)
            return false;
        mapped = mapper_helpers::mapPrg32k(addr, prg, prgBanks);
        return true;
    }
    void cpuMapWrite(uint16_t addr, uint32_t &, uint8_t data) override {
        if (addr >= 0x4020 && addr < 0x6000) {
            prg = (uint8_t)((data >> 4) & 0x0F);
            chr = (uint8_t)(data & 0x0F);
        }
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
    const char* name() const override { return "Jing Ke Xin Zhuan / Sheng Huo Lie Zhuan"; }
};

REGISTER_MAPPER(240, Mapper240)
