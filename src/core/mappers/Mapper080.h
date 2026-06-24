#pragma once
#include "MapperBase.h"
#include "MapperRegistry.h"
#include <array>

// ----------------------------------------------------------------------------
// Mapper 080 - Taito X1-005 (Minelvaton Saga, Taito Grand Prix)
// ----------------------------------------------------------------------------
// Rejestry w $7EF0-$7EFF (CPU read/write):
//   $7EF0..$7EF5: CHR regs (2K x2 + 1K x4)
//   $7EF6/$7EF7: [.... ...M] mirroring (0=H,1=V)
//   $7EF8/$7EF9: $A3 odblokowuje wewnetrzny 128B RAM @ $7F00-$7FFF
//   $7EFA/$7EFB: PRG reg 0 (8K @ $8000)
//   $7EFC/$7EFD: PRG reg 1 (8K @ $A000)
//   $7EFE/$7EFF: PRG reg 2 (8K @ $C000)
// PRG: { R0, R1, R2, last-8K }.
// CHR: 2K <$7EF0> @ $0000, 2K <$7EF1> @ $0800, R2..R5 (1K) @ $1000..$1C00.
// ----------------------------------------------------------------------------
class Mapper080 : public Mapper {
public:
    using Mapper::Mapper;

    void reset() override {
        chr.fill(0);
        prg.fill(0);
        ram.fill(0);
        mirrorMode = Mirroring::HORIZONTAL;
        ramEnabled = false;
    }

    bool cpuMapRead(uint16_t addr, uint32_t &mapped, uint8_t &data) override {
        if (addr >= 0x7F00 && addr <= 0x7FFF) {
            if (ramEnabled)
                data = ram[addr & 0x7F];
            else
                data = 0xFF;
            mapped = 0xFFFFFFFFu; // mapper dostarczyl dane
            return true;
        }
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
        if (addr >= 0x7F00 && addr <= 0x7FFF) {
            if (ramEnabled)
                ram[addr & 0x7F] = data;
            return;
        }
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
        case 0x7:
            mirrorMode =
                (data & 0x01) ? Mirroring::VERTICAL : Mirroring::HORIZONTAL;
            break;
        case 0x8:
        case 0x9:
            ramEnabled = (data == 0xA3);
            break;
        case 0xA:
        case 0xB:
            prg[0] = data;
            break;
        case 0xC:
        case 0xD:
            prg[1] = data;
            break;
        case 0xE:
        case 0xF:
            prg[2] = data;
            break;
        }
    }
    bool ppuMapRead(uint16_t addr, uint32_t &mapped) override {
        if (addr > 0x1FFF)
            return false;
        if (addr < 0x0800) {
            mapped = (uint32_t)(chr[0] & 0xFE) * 0x0400 + (addr & 0x07FF);
        } else if (addr < 0x1000) {
            mapped = (uint32_t)(chr[1] & 0xFE) * 0x0400 + (addr & 0x07FF);
        } else {
            uint8_t which = (uint8_t)(2 + ((addr >> 10) & 0x03));
            mapped = (uint32_t)chr[which] * 0x0400 + (addr & 0x03FF);
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
    std::array<uint8_t, 128> ram{};
    Mirroring mirrorMode = Mirroring::HORIZONTAL;
    bool ramEnabled = false;
};

REGISTER_MAPPER(80, Mapper080)
// Mapper 207 (Fudou Myouou Den) - bardzo zblizony, brak 1-screen variant. Alias
// roboczy:
REGISTER_MAPPER_AS(207, Mapper080, alias207)
