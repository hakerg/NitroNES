#pragma once
#include "MapperBase.h"
#include "MapperRegistry.h"
#include <array>

// ----------------------------------------------------------------------------
// Mapper 164 - Final Fantasy V (pirate)
// ----------------------------------------------------------------------------
// Pojedynczy rejestr (mask $F300) wybiera 32 KB PRG @ $8000.
// CHR-RAM (typowo 8 KB). Brak IRQ / mirroring control.
// Reset: rejestr = $FF -> ostatnia 32 KB strona.
// ----------------------------------------------------------------------------
class Mapper164 : public Mapper {
public:
    Mapper164(uint8_t prg, uint8_t chr) : Mapper(prg, chr) { reset(); }

    void reset() override { reg = 0xFF; }

    bool cpuMapRead(uint16_t addr, uint32_t &mapped, uint8_t &) override {
        if (addr < 0x8000)
            return false;
        mapped = mapper_helpers::mapPrg32k(addr, reg, prgBanks);
        return true;
    }

    void cpuMapWrite(uint16_t addr, uint32_t &, uint8_t data) override {
        if (addr < 0x5000)
            return;
        if ((addr & 0xF300) == 0x5000 || (addr & 0xF300) == 0xD000) {
            reg = data;
        }
    }

    bool ppuMapRead(uint16_t addr, uint32_t &mapped) override {
        if (addr > 0x1FFF)
            return false;
        mapped = mapper_helpers::mapChr8k(addr, 0, chrBanks);
        return true;
    }
    bool ppuMapWrite(uint16_t addr, uint32_t &mapped) override {
        return mapper_helpers::chrRamWrite(addr, mapped, chrBanks);
    }

private:
    uint8_t reg = 0xFF;
public:
    std::unique_ptr<Mapper> clone() const override { return std::make_unique<Mapper164>(*this); }
    const char* name() const override { return "Final Fantasy V (pirate)"; }
};

REGISTER_MAPPER(164, Mapper164)
