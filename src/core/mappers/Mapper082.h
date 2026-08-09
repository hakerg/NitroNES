#pragma once
#include "MapperBase.h"
#include "MapperRegistry.h"
#include <array>

// ----------------------------------------------------------------------------
// Mapper 082 - Taito X1-017 (SD Keiji Blader, Kyuukyoku Harikiri Stadium)
// ----------------------------------------------------------------------------
// Rejestry @ $7EFx:
//   $7EF0-$7EF5: CHR Regs (2K x2 + 1K x4)
//   $7EF6: [.... ..CM]  C = CHR mode select (swap halves), M = mirror (0=H,1=V)
//   $7EFA: PRG reg 0 (8K @ $8000) -- written value >>2
//   $7EFB: PRG reg 1 (8K @ $A000) -- written value >>2
//   $7EFC: PRG reg 2 (8K @ $C000) -- written value >>2
// PRG bank values used as (data >> 2).
// CHR mode 0: 2K,2K,1K,1K,1K,1K (jak mapper 080)
// CHR mode 1: 1K,1K,1K,1K,2K,2K (zamiana polowek)
// ----------------------------------------------------------------------------
class Mapper082 : public Mapper {
public:
    using Mapper::Mapper;

    void reset() override {
        chr.fill(0);
        prg.fill(0);
        mirrorMode = Mirroring::HORIZONTAL;
        chrMode = false;
    }

    bool cpuMapRead(uint16_t addr, uint32_t &mapped, uint8_t &) override {
        if (addr < 0x8000)
            return false;
        uint8_t last = (uint8_t)(prgBanks * 2 - 1);
        uint8_t bank;
        if (addr < 0xA000)
            bank = prg[0];
        else if (addr < 0xC000)
            bank = prg[1];
        else if (addr < 0xE000)
            bank = prg[2];
        else
            bank = last;
        mapped = (uint32_t)bank * 0x2000 + (addr & 0x1FFF);
        return true;
    }
    void cpuMapWrite(uint16_t addr, uint32_t &, uint8_t data) override {
        if (addr < 0x7EF0 || addr > 0x7EFF)
            return;
        uint16_t r = addr & 0x000F;
        switch (r) {
        case 0x0:
        case 0x1:
        case 0x2:
        case 0x3:
        case 0x4:
        case 0x5:
            chr[r] = data;
            break;
        case 0x6:
            mirrorMode =
                (data & 0x01) ? Mirroring::VERTICAL : Mirroring::HORIZONTAL;
            chrMode = (data & 0x02) != 0;
            break;
        case 0xA:
            prg[0] = (uint8_t)(data >> 2);
            break;
        case 0xB:
            prg[1] = (uint8_t)(data >> 2);
            break;
        case 0xC:
            prg[2] = (uint8_t)(data >> 2);
            break;
        default:
            break;
        }
    }
    bool ppuMapRead(uint16_t addr, uint32_t &mapped) override {
        if (addr > 0x1FFF)
            return false;
        // Mode 0: [<R0>][<R1>][R2][R3][R4][R5]
        // Mode 1: [R2][R3][R4][R5][<R0>][<R1>]
        uint16_t a = addr;
        if (chrMode)
            a = (uint16_t)(a ^ 0x1000);
        if (a < 0x0800) {
            mapped = (uint32_t)(chr[0] & 0xFE) * 0x0400 + (a & 0x07FF);
        } else if (a < 0x1000) {
            mapped = (uint32_t)(chr[1] & 0xFE) * 0x0400 + (a & 0x07FF);
        } else {
            uint8_t which = (uint8_t)(2 + ((a >> 10) & 0x03));
            mapped = (uint32_t)chr[which] * 0x0400 + (a & 0x03FF);
        }
        return true;
    }
    bool ppuMapWrite(uint16_t addr, uint32_t &mapped) override {
        return mapper_helpers::chrRamWrite(addr, mapped, chrBanks);
    }

    Mirroring mirror() const override { return mirrorMode; }
    bool hasDynamicMirror() const override { return true; }

private:
    std::array<uint8_t, 6> chr{};
    std::array<uint8_t, 3> prg{};
    Mirroring mirrorMode = Mirroring::HORIZONTAL;
    bool chrMode = false;
public:
    std::unique_ptr<Mapper> clone() const override { return std::make_unique<Mapper082>(*this); }
    const char* name() const override { return "Taito X1-017 (SD Keiji Blader, Kyuukyoku Harikiri Stadium)"; }
};

REGISTER_MAPPER(82, Mapper082)
