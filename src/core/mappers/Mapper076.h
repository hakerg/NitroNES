#pragma once
#include "MapperBase.h"
#include "MapperRegistry.h"
#include <array>

// Mapper 076 - Namcot 108 (wariant; bez nametable RAM control).
class Mapper076 : public Mapper {
public:
    using Mapper::Mapper;
    void reset() override {
        bankSel = 0;
        for (auto &r : R)
            r = 0;
    }
    bool cpuMapRead(uint16_t a, uint32_t &mapped, uint8_t &) override {
        if (a < 0x8000)
            return false;
        uint32_t total8k = (uint32_t)prgBanks * 2;
        uint8_t b;
        if (a < 0xA000)
            b = R[6] & 0x3F;
        else if (a < 0xC000)
            b = R[7] & 0x3F;
        else if (a < 0xE000)
            b = (uint8_t)(total8k - 2);
        else
            b = (uint8_t)(total8k - 1);
        mapped = (uint32_t)b * 0x2000 + (a & 0x1FFF);
        return true;
    }
    void cpuMapWrite(uint16_t a, uint32_t &, uint8_t data) override {
        if (a < 0x8000)
            return;
        if ((a & 1) == 0)
            bankSel = data & 0x07;
        else
            R[bankSel] = data;
    }
    bool ppuMapRead(uint16_t a, uint32_t &mapped) override {
        if (a > 0x1FFF)
            return false;
        uint8_t b;
        if (a < 0x0800)
            b = R[2] & 0x3F;
        else if (a < 0x1000)
            b = R[3] & 0x3F;
        else if (a < 0x1800)
            b = R[4] & 0x3F;
        else
            b = R[5] & 0x3F;
        mapped = (uint32_t)b * 0x0800 + (a & 0x07FF);
        return true;
    }
    bool ppuMapWrite(uint16_t a, uint32_t &mapped) override {
        return mapper_helpers::chrRamWrite(a, mapped, chrBanks);
    }

private:
    uint8_t bankSel = 0;
    std::array<uint8_t, 8> R{};
    const char* name() const override { return "Namcot 108 (wariant; bez nametable RAM control)."; }
};

REGISTER_MAPPER(76, Mapper076)
