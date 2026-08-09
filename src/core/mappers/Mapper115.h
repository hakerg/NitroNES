#pragma once
#include "Mapper004.h"
#include "MapperRegistry.h"

// ----------------------------------------------------------------------------
// Mapper 115 - MMC3 wariant (Yuu Yuu Hakusho)
// ----------------------------------------------------------------------------
// Rejestry @ $6000-$7FFF (mask $6001):
//   $6000: [O... PPPP]  O = PRG mode, P = 16 KB PRG page do $8000-BFFF
//   $6001: [.... ...C]  C = CHR block (OR 0x000 / 0x100 dla CHR-1k page)
// $8000-FFFF: identycznie jak MMC3.
// Gdy O=1, MMC3 PRG dla $8000-BFFF jest pomijany; w innym razie standardowy
// MMC3.
// ----------------------------------------------------------------------------
class Mapper115 : public Mapper004 {
public:
    using Mapper004::Mapper004;

    void reset() override {
        Mapper004::reset();
        extPrgMode = false;
        extPrgPage = 0;
        chrBlock = 0;
    }

    void cpuMapWrite(uint16_t addr, uint32_t &mapped, uint8_t data) override {
        if (addr >= 0x6000 && addr <= 0x7FFF) {
            if (addr & 0x0001) {
                chrBlock = data & 0x01;
            } else {
                extPrgMode = (data & 0x80) != 0;
                extPrgPage = data & 0x0F;
            }
            return;
        }
        Mapper004::cpuMapWrite(addr, mapped, data);
    }

    bool cpuMapRead(uint16_t addr, uint32_t &mapped, uint8_t &data) override {
        if (extPrgMode && addr >= 0x8000 && addr < 0xC000) {
            uint8_t bank16 = mapper_helpers::maskBank(extPrgPage, prgBanks);
            mapped = (uint32_t)bank16 * 0x4000 + (addr & 0x3FFF);
            return true;
        }
        return Mapper004::cpuMapRead(addr, mapped, data);
    }

    bool ppuMapRead(uint16_t addr, uint32_t &mapped) override {
        if (!Mapper004::ppuMapRead(addr, mapped))
            return false;
        if (chrBanks == 0)
            return true;
        // CHR-OR z $000 lub $100 (1 KB jednostki).
        uint32_t offsetWithin1k = mapped & 0x03FF;
        uint32_t bank1k = mapped >> 10;
        bank1k = (bank1k & 0xFF) | ((uint32_t)chrBlock << 8);
        uint32_t total1k = (uint32_t)chrBanks * 8;
        if (total1k)
            bank1k %= total1k;
        mapped = bank1k * 0x0400 + offsetWithin1k;
        return true;
    }

private:
    bool extPrgMode = false;
    uint8_t extPrgPage = 0;
    uint8_t chrBlock = 0;
public:
    std::unique_ptr<Mapper> clone() const override { return std::make_unique<Mapper115>(*this); }
    const char* name() const override { return "MMC3 wariant (Yuu Yuu Hakusho)"; }
};

REGISTER_MAPPER(115, Mapper115)
