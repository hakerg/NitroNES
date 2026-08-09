#pragma once
#include "MapperBase.h"
#include "MapperRegistry.h"

// Mapper 079 - NINA-03/06 (American Video Entertainment).
class Mapper079 : public Mapper {
public:
    using Mapper::Mapper;
    void reset() override {
        prg = 0;
        chr = 0;
    }
    bool cpuMapRead(uint16_t a, uint32_t &mapped, uint8_t &) override {
        if (a < 0x8000)
            return false;
        mapped = (uint32_t)prg * 0x8000 + (a & 0x7FFF);
        return true;
    }
    void cpuMapWrite(uint16_t a, uint32_t &, uint8_t data) override {
        if (a >= 0x4100 && a <= 0x5FFF && (a & 0x4100) == 0x4100) {
            prg = (data >> 3) & 0x01;
            chr = data & 0x07;
            if (data & 0x40)
                chr |= 0x08; // high bit
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
    uint8_t prg = 0, chr = 0;
public:
    std::unique_ptr<Mapper> clone() const override { return std::make_unique<Mapper079>(*this); }
    const char* name() const override { return "NINA-03/06 (American Video Entertainment)."; }
};

REGISTER_MAPPER(79, Mapper079)
