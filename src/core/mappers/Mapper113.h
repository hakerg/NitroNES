#pragma once
#include "MapperBase.h"
#include "MapperRegistry.h"

// Mapper 113 - HES.
class Mapper113 : public Mapper {
public:
    using Mapper::Mapper;
    void reset() override {
        prg = 0;
        chr = 0;
        mirrorMode = Mirroring::HORIZONTAL;
    }
    bool cpuMapRead(uint16_t a, uint32_t &mapped, uint8_t &) override {
        if (a < 0x8000)
            return false;
        mapped = (uint32_t)prg * 0x8000 + (a & 0x7FFF);
        return true;
    }
    void cpuMapWrite(uint16_t a, uint32_t &, uint8_t data) override {
        if (a >= 0x4100 && a <= 0x5FFF && (a & 0x4100) == 0x4100) {
            prg = (data >> 3) & 0x07;
            chr = (uint8_t)((data & 0x07) | ((data >> 3) & 0x08));
            mirrorMode =
                (data & 0x80) ? Mirroring::VERTICAL : Mirroring::HORIZONTAL;
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
    Mirroring mirror() const override { return mirrorMode; }
    bool hasDynamicMirror() const override { return true; }

private:
    uint8_t prg = 0, chr = 0;
    Mirroring mirrorMode = Mirroring::HORIZONTAL;
public:
    std::unique_ptr<Mapper> clone() const override { return std::make_unique<Mapper113>(*this); }
    const char* name() const override { return "HES."; }
};

REGISTER_MAPPER(113, Mapper113)
