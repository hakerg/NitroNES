#pragma once
#include "Mapper004.h"
#include "MapperRegistry.h"

// ----------------------------------------------------------------------------
// Mapper 205 - MMC3 multicart (15-in-1, 3-in-1)
// ----------------------------------------------------------------------------
// Multicart reg @ $6000-$7FFF: [.... ..MM]  M = blok (0..3).
// Banki MMC3 sa AND-owane i OR-owane wedlug tabeli:
//   Block  PRG-AND  PRG-OR  CHR-AND  CHR-OR
//     0     $1F      $00     $FF      $000
//     1     $1F      $10     $FF      $080
//     2     $0F      $20     $7F      $100
//     3     $0F      $30     $7F      $180
// PRG-AND/OR sa w jednostkach 8 KB; CHR-AND/OR w jednostkach 1 KB.
// ----------------------------------------------------------------------------
class Mapper205 : public Mapper004 {
public:
    using Mapper004::Mapper004;

    void reset() override {
        Mapper004::reset();
        block = 0;
    }

    void cpuMapWrite(uint16_t addr, uint32_t &mapped, uint8_t data) override {
        if (addr >= 0x6000 && addr <= 0x7FFF) {
            block = data & 0x03;
            return;
        }
        Mapper004::cpuMapWrite(addr, mapped, data);
    }

    bool cpuMapRead(uint16_t addr, uint32_t &mapped, uint8_t &data) override {
        if (!Mapper004::cpuMapRead(addr, mapped, data))
            return false;
        if (addr < 0x8000)
            return true;
        static const uint8_t prgAnd[4] = {0x1F, 0x1F, 0x0F, 0x0F};
        static const uint8_t prgOr[4] = {0x00, 0x10, 0x20, 0x30};
        uint32_t bank8 = (mapped >> 13);
        uint32_t off = mapped & 0x1FFF;
        bank8 = (bank8 & prgAnd[block]) | prgOr[block];
        uint8_t total8k = (uint8_t)(prgBanks * 2);
        if (total8k)
            bank8 %= total8k;
        mapped = bank8 * 0x2000 + off;
        return true;
    }

    bool ppuMapRead(uint16_t addr, uint32_t &mapped) override {
        if (!Mapper004::ppuMapRead(addr, mapped))
            return false;
        if (chrBanks == 0)
            return true;
        static const uint32_t chrAnd[4] = {0xFF, 0xFF, 0x7F, 0x7F};
        static const uint32_t chrOr[4] = {0x000, 0x080, 0x100, 0x180};
        uint32_t bank1k = mapped >> 10;
        uint32_t off = mapped & 0x03FF;
        bank1k = (bank1k & chrAnd[block]) | chrOr[block];
        uint32_t total1k = (uint32_t)chrBanks * 8;
        if (total1k)
            bank1k %= total1k;
        mapped = bank1k * 0x0400 + off;
        return true;
    }

private:
    uint8_t block = 0;
};

REGISTER_MAPPER(205, Mapper205)
