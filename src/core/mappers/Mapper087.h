#pragma once
#include "MapperBase.h"
#include "MapperRegistry.h"

// Mapper 087 - Jaleco/Konami. Rejestr w $6000-7FFF (zamiast SRAM).
class Mapper087 : public Mapper {
public:
    using Mapper::Mapper;
    void reset() override { chr = 0; }
    bool cpuMapRead(uint16_t a, uint32_t &mapped, uint8_t &) override {
        if (a < 0x8000)
            return false;
        mapped = a & 0x7FFF;
        return true;
    }
    void cpuMapWrite(uint16_t a, uint32_t &, uint8_t data) override {
        if (a >= 0x6000 && a <= 0x7FFF) {
            chr = (uint8_t)(((data & 0x01) << 1) | ((data & 0x02) >> 1));
        }
    }
    bool ppuMapRead(uint16_t a, uint32_t &mapped) override {
        if (a > 0x1FFF)
            return false;
        mapped = (uint32_t)chr * 0x2000 + (a & 0x1FFF);
        return true;
    }
    bool ppuMapWrite(uint16_t a, uint32_t &mapped) override {
        return mapper_helpers::chrRamWrite(a, mapped, chrBanks);
    }

private:
    uint8_t chr = 0;
public:
    std::unique_ptr<Mapper> clone() const override { return std::make_unique<Mapper087>(*this); }
    const char* name() const override { return "Jaleco/Konami. Rejestr w $6000-7FFF (zamiast SRAM)."; }
};

REGISTER_MAPPER(87, Mapper087)
