#pragma once
#include "MapperBase.h"
#include "MapperRegistry.h"
#include <array>

// Mapper 112 - Asder/NTDEC. PRG 2x8k + 2x fixed, CHR 2x2k + 4x1k.
class Mapper112 : public Mapper {
public:
    using Mapper::Mapper;
    void reset() override {
        sel = 0;
        for (auto &r : R)
            r = 0;
        mirrorMode = Mirroring::VERTICAL;
    }
    bool cpuMapRead(uint16_t a, uint32_t &mapped, uint8_t &) override {
        if (a < 0x8000)
            return false;
        uint32_t total8k = (uint32_t)prgBanks * 2;
        uint8_t b;
        if (a < 0xA000)
            b = R[0];
        else if (a < 0xC000)
            b = R[1];
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
        uint16_t reg = a & 0xE001;
        if (reg == 0x8000)
            sel = data & 0x07;
        else if (reg == 0xA000) {
            if (sel < 8)
                R[sel] = data;
        } else if (reg == 0xE000)
            mirrorMode =
                (data & 0x01) ? Mirroring::HORIZONTAL : Mirroring::VERTICAL;
    }
    bool ppuMapRead(uint16_t a, uint32_t &mapped) override {
        if (a > 0x1FFF)
            return false;
        uint8_t b;
        if (a < 0x0800)
            b = R[2] & 0xFE;
        else if (a < 0x1000)
            b = R[3] & 0xFE;
        else if (a < 0x1400) {
            mapped = (uint32_t)R[4] * 0x0400 + (a & 0x03FF);
            return true;
        } else if (a < 0x1800) {
            mapped = (uint32_t)R[5] * 0x0400 + (a & 0x03FF);
            return true;
        } else if (a < 0x1C00) {
            mapped = (uint32_t)R[6] * 0x0400 + (a & 0x03FF);
            return true;
        } else {
            mapped = (uint32_t)R[7] * 0x0400 + (a & 0x03FF);
            return true;
        }
        mapped = (uint32_t)b * 0x0400 + (a & 0x07FF);
        return true;
    }
    bool ppuMapWrite(uint16_t a, uint32_t &mapped) override {
        return mapper_helpers::chrRamWrite(a, mapped, chrBanks);
    }
    Mirroring mirror() const override { return mirrorMode; }
    bool hasDynamicMirror() const override { return true; }

private:
    uint8_t sel = 0;
    std::array<uint8_t, 8> R{};
    Mirroring mirrorMode = Mirroring::VERTICAL;
    const char* name() const override { return "Asder/NTDEC. PRG 2x8k + 2x fixed, CHR 2x2k + 4x1k."; }
};

REGISTER_MAPPER(112, Mapper112)
