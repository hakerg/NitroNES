#pragma once
#include "MapperBase.h"
#include "MapperRegistry.h"
#include <array>

// Mapper 068 - Sunsoft-4. CHR-ROM jako nametables (uproszczone: brak NT-ROM).
class Mapper068 : public Mapper {
public:
    using Mapper::Mapper;
    void reset() override {
        for (auto &c : chr)
            c = 0;
        prg = 0;
        mirrorMode = Mirroring::VERTICAL;
    }
    bool cpuMapRead(uint16_t a, uint32_t &mapped, uint8_t &) override {
        if (a < 0x8000)
            return false;
        if (a < 0xC000)
            mapped = (uint32_t)prg * 0x4000 + (a & 0x3FFF);
        else
            mapped = (uint32_t)(prgBanks - 1) * 0x4000 + (a & 0x3FFF);
        return true;
    }
    void cpuMapWrite(uint16_t a, uint32_t &, uint8_t data) override {
        if (a < 0x8000)
            return;
        switch (a & 0xF000) {
        case 0x8000:
            chr[0] = data;
            break;
        case 0x9000:
            chr[1] = data;
            break;
        case 0xA000:
            chr[2] = data;
            break;
        case 0xB000:
            chr[3] = data;
            break;
        case 0xE000:
            mirrorMode =
                (data & 0x01) ? Mirroring::HORIZONTAL : Mirroring::VERTICAL;
            break;
        case 0xF000:
            prg = data & 0x0F;
            break;
        default:
            break;
        }
    }
    bool ppuMapRead(uint16_t a, uint32_t &mapped) override {
        if (a > 0x1FFF)
            return false;
        uint8_t idx = (uint8_t)(a >> 11);
        mapped = (uint32_t)chr[idx] * 0x0800 + (a & 0x07FF);
        return true;
    }
    bool ppuMapWrite(uint16_t a, uint32_t &mapped) override {
        return mapper_helpers::chrRamWrite(a, mapped, chrBanks);
    }
    Mirroring mirror() const override { return mirrorMode; }
    bool hasDynamicMirror() const override { return true; }

private:
    std::array<uint8_t, 4> chr{};
    uint8_t prg = 0;
    Mirroring mirrorMode = Mirroring::VERTICAL;
};

REGISTER_MAPPER(68, Mapper068)
