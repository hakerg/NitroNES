#pragma once
#include "MapperBase.h"
#include "MapperRegistry.h"
#include <array>

// ----------------------------------------------------------------------------
// Mapper 088 - Namco 118 / Namcot 3433 (Quinty, Dragon Spirit)
// ----------------------------------------------------------------------------
// MMC3-like address/data port:
//   $8000: [.... .AAA] target register
//   $8001: [DDDD DDDD] data
// CHR: 2x 2 KB @ $0000,$0800 + 4x 1 KB @ $1000..$1C00.
//      $0xxx -> bank z pierwszych 64 KB (AND $3F),
//      $1xxx -> bank z drugich 64 KB    (OR  $40).
// PRG: 2x 8 KB switchable @ $8000,$A000 + {-2,-1} fixed.
// Mirroring: hardwired (z naglowka).
// ----------------------------------------------------------------------------
class Mapper088 : public Mapper {
public:
    using Mapper::Mapper;

    void reset() override {
        target = 0;
        regs.fill(0);
    }

    bool cpuMapRead(uint16_t addr, uint32_t &mapped, uint8_t &) override {
        if (addr < 0x8000)
            return false;
        uint8_t last = (uint8_t)(prgBanks * 2 - 1);
        uint8_t prev = (uint8_t)(prgBanks * 2 - 2);
        uint8_t bank;
        if (addr < 0xA000)
            bank = regs[6] & 0x3F;
        else if (addr < 0xC000)
            bank = regs[7] & 0x3F;
        else if (addr < 0xE000)
            bank = prev;
        else
            bank = last;
        mapped = (uint32_t)bank * 0x2000 + (addr & 0x1FFF);
        return true;
    }

    void cpuMapWrite(uint16_t addr, uint32_t &, uint8_t data) override {
        if (addr < 0x8000)
            return;
        if ((addr & 0x8001) == 0x8000) {
            target = data & 0x07;
        } else if ((addr & 0x8001) == 0x8001) {
            regs[target] = data;
        }
    }

    bool ppuMapRead(uint16_t addr, uint32_t &mapped) override {
        if (addr > 0x1FFF)
            return false;
        uint8_t bank;
        uint32_t off;
        if (addr < 0x0800) { // 2K @ $0000 - <R:0> AND $3F
            bank = (uint8_t)((regs[0] & 0x3E));
            off = (uint32_t)bank * 0x0400 + (addr & 0x07FF);
        } else if (addr < 0x1000) { // 2K @ $0800 - <R:1> AND $3F
            bank = (uint8_t)((regs[1] & 0x3E));
            off = (uint32_t)bank * 0x0400 + (addr & 0x07FF);
        } else { // 1K @ $1000..$1FFF - R:2..R:5 OR $40
            uint8_t which = (uint8_t)((addr >> 10) & 0x03); // 0..3
            bank = (uint8_t)(regs[2 + which] | 0x40);
            off = (uint32_t)bank * 0x0400 + (addr & 0x03FF);
        }
        mapped = off;
        return true;
    }
    bool ppuMapWrite(uint16_t addr, uint32_t &mapped) override {
        return mapper_helpers::chrRamWrite(addr, mapped, chrBanks);
    }

private:
    uint8_t target = 0;
    std::array<uint8_t, 8> regs{};
};

REGISTER_MAPPER(88, Mapper088)
