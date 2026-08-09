#pragma once
#include "MapperBase.h"
#include "MapperRegistry.h"

// Mapper 184 - Sunsoft-1. Rejestr w $6000-7FFF.
class Mapper184 : public Mapper {
public:
    using Mapper::Mapper;
    void reset() override {
        chr0 = 0;
        chr1 = 4;
    }
    bool cpuMapRead(uint16_t a, uint32_t &mapped, uint8_t &) override {
        if (a < 0x8000)
            return false;
        mapped = a & 0x7FFF;
        return true;
    }
    void cpuMapWrite(uint16_t a, uint32_t &, uint8_t data) override {
        if (a >= 0x6000 && a <= 0x7FFF) {
            chr0 = data & 0x07;
            chr1 = (uint8_t)(((data >> 4) & 0x07) |
                             0x04); // wysoki bit zawsze ustawiony
        }
    }
    bool ppuMapRead(uint16_t a, uint32_t &mapped) override {
        if (a > 0x1FFF)
            return false;
        if (a < 0x1000)
            mapped = (uint32_t)chr0 * 0x1000 + (a & 0x0FFF);
        else
            mapped = (uint32_t)chr1 * 0x1000 + (a & 0x0FFF);
        return true;
    }
    bool ppuMapWrite(uint16_t a, uint32_t &mapped) override {
        return mapper_helpers::chrRamWrite(a, mapped, chrBanks);
    }

private:
    uint8_t chr0 = 0, chr1 = 4;
public:
    std::unique_ptr<Mapper> clone() const override { return std::make_unique<Mapper184>(*this); }
    const char* name() const override { return "Sunsoft-1. Rejestr w $6000-7FFF."; }
};

REGISTER_MAPPER(184, Mapper184)
