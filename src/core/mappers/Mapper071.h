#pragma once
#include "MapperBase.h"
#include "MapperRegistry.h"

// ----------------------------------------------------------------------------
// Mapper 071 - Camerica / Codemasters (UNROM-podobny)
// ----------------------------------------------------------------------------
class Mapper071 : public Mapper {
public:
    Mapper071(uint8_t prg, uint8_t chr) : Mapper(prg, chr) {
        prgBankHi = prg - 1;
    }

    void reset() override {
        prgBankLo = 0;
        prgBankHi = prgBanks - 1;
    }

    bool cpuMapRead(uint16_t addr, uint32_t &mapped, uint8_t &) override {
        if (addr >= 0x8000 && addr < 0xC000) {
            mapped = (uint32_t)prgBankLo * 0x4000 + (addr & 0x3FFF);
            return true;
        }
        if (addr >= 0xC000) {
            mapped = (uint32_t)prgBankHi * 0x4000 + (addr & 0x3FFF);
            return true;
        }
        return false;
    }
    void cpuMapWrite(uint16_t addr, uint32_t &, uint8_t data) override {
        if (addr >= 0x8000 && addr <= 0x9FFF) {
            // Fire Hawk: 1-screen mirroring (bit 4)
            mirrorMode = (data & 0x10) ? Mirroring::ONESCREEN_HI
                                       : Mirroring::ONESCREEN_LO;
            hasMirror = true;
        }
        if (addr >= 0xC000) {
            prgBankLo = data & 0x0F;
        }
    }
    bool ppuMapRead(uint16_t addr, uint32_t &mapped) override {
        if (addr <= 0x1FFF) {
            mapped = addr;
            return true;
        }
        return false;
    }
    bool ppuMapWrite(uint16_t addr, uint32_t &mapped) override {
        return mapper_helpers::chrRamWrite(addr, mapped, chrBanks);
    }
    Mirroring mirror() const override { return mirrorMode; }
    bool hasDynamicMirror() const override { return hasMirror; }

private:
    uint8_t prgBankLo = 0, prgBankHi = 0;
    bool hasMirror = false;
    Mirroring mirrorMode = Mirroring::ONESCREEN_LO;
    const char* name() const override { return "Camerica / Codemasters (UNROM-podobny)"; }
};

REGISTER_MAPPER(71, Mapper071)
