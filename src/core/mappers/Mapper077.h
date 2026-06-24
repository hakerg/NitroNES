#pragma once
#include "MapperBase.h"
#include "MapperRegistry.h"

// ----------------------------------------------------------------------------
// Mapper 077 - Irem (Napoleon Senki)
// ----------------------------------------------------------------------------
// PRG: 32 KB switchable. CHR: 2 KB ROM @ $0000-$07FF + 8 KB CHR-RAM @
// $0800-$1FFF. Mirroring: four-screen (sprzetowo). Tutaj zwracamy FOURSCREEN.
// $8000-FFFF: [CCCC PPPP] - C = CHR reg, P = PRG reg (32 KB).
// ----------------------------------------------------------------------------
class Mapper077 : public Mapper {
public:
    using Mapper::Mapper;

    void reset() override {
        prg = 0;
        chr = 0;
    }

    bool cpuMapRead(uint16_t addr, uint32_t &mapped, uint8_t &) override {
        if (addr < 0x8000)
            return false;
        mapped = mapper_helpers::mapPrg32k(addr, prg, prgBanks);
        return true;
    }
    void cpuMapWrite(uint16_t addr, uint32_t &, uint8_t data) override {
        if (addr < 0x8000)
            return;
        prg = data & 0x0F;
        chr = (data >> 4) & 0x0F;
    }
    bool ppuMapRead(uint16_t addr, uint32_t &mapped) override {
        if (addr > 0x1FFF)
            return false;
        if (addr < 0x0800) {
            // CHR-ROM: 2 KB swap z banku 'chr' (jednostki 2 KB)
            mapped = (uint32_t)chr * 0x0800 + (addr & 0x07FF);
            return true;
        }
        // CHR-RAM @ $0800-$1FFF - mapowanie 1:1
        mapped = addr;
        return true;
    }
    bool ppuMapWrite(uint16_t addr, uint32_t &mapped) override {
        if (addr >= 0x0800 && addr <= 0x1FFF) {
            mapped = addr;
            return true;
        }
        return false;
    }
    Mirroring mirror() const override { return Mirroring::FOURSCREEN; }
    bool hasBusConflicts() const override { return true; }

private:
    uint8_t prg = 0, chr = 0;
};

REGISTER_MAPPER(77, Mapper077)
