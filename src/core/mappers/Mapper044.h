#pragma once
#include "Mapper004.h"

// ----------------------------------------------------------------------------
// Mapper 044 - MMC3 multicart z 7 blokami po 128K PRG/CHR (blok 6 = 256K).
//   $A001 [.... .BBB] (po wlaczeniu RAM-write enable, jak MMC3) wybiera blok.
// Aby unikac kolizji z normalnym MMC3 PRG-RAM protect, czytamy bity
// bezposrednio.
// ----------------------------------------------------------------------------
class Mapper044 : public Mapper004 {
public:
    using Mapper004::Mapper004;

    void reset() override {
        Mapper004::reset();
        block = 0;
    }

    void cpuMapWrite(uint16_t addr, uint32_t &mapped, uint8_t data) override {
        if (addr >= 0xA000 && addr < 0xC000 && (addr & 1)) {
            // $A001: bity 0..2 = blok
            block = data & 0x07;
            if (block > 6)
                block = 6;
            Mapper004::cpuMapWrite(addr, mapped, data);
            return;
        }
        Mapper004::cpuMapWrite(addr, mapped, data);
    }

    bool cpuMapRead(uint16_t addr, uint32_t &mapped, uint8_t &data) override {
        if (!Mapper004::cpuMapRead(addr, mapped, data))
            return false;
        // Zaaplikuj OR bloku do offsetu PRG (8K paginacja MMC3).
        uint32_t prgAnd = (block == 6) ? 0x1FFFF : 0x0FFFF; // 128K vs 256K
        uint32_t prgOr;
        if (block <= 5)
            prgOr = (uint32_t)block * 128 * 1024;
        else
            prgOr = 6u * 128u * 1024u;
        mapped = (mapped & prgAnd) | prgOr;
        return true;
    }

    bool ppuMapRead(uint16_t addr, uint32_t &mapped) override {
        if (!Mapper004::ppuMapRead(addr, mapped))
            return false;
        uint32_t chrAnd = (block == 6) ? 0x3FFFF : 0x1FFFF; // 256K vs 128K
        uint32_t chrOr;
        if (block <= 5)
            chrOr = (uint32_t)block * 128 * 1024;
        else
            chrOr = 6u * 128u * 1024u;
        mapped = (mapped & chrAnd) | chrOr;
        return true;
    }

private:
    uint8_t block = 0;
public:
    std::unique_ptr<Mapper> clone() const override { return std::make_unique<Mapper044>(*this); }
    const char* name() const override { return "MMC3 multicart z 7 blokami po 128K PRG/CHR (blok 6 = 256K)."; }
};

REGISTER_MAPPER(44, Mapper044)
