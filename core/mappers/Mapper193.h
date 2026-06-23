#pragma once
#include "MapperBase.h"
#include "MapperRegistry.h"
#include <array>

// ----------------------------------------------------------------------------
// Mapper 193 - NTDEC TC-112 (Fighting Hero)
// ----------------------------------------------------------------------------
// Rejestry @ $6000-$7FFF (mask $6003):
//   $6000: CHR Reg 0 (4 KB @ $0000)
//   $6001: CHR Reg 1 (2 KB @ $1000)
//   $6002: CHR Reg 2 (2 KB @ $1800)
//   $6003: PRG Reg   (8 KB @ $8000)
// PRG: [$6003] [-3] [-2] [-1]   (8 KB pages)
// Brak SRAM. CHR-ROM tylko.
// ----------------------------------------------------------------------------
class Mapper193 : public Mapper {
public:
    Mapper193(uint8_t prg, uint8_t chr) : Mapper(prg, chr) { reset(); }

    void reset() override {
        chr0 = chr1 = chr2 = 0;
        prgReg = 0;
    }

    bool cpuMapRead(uint16_t addr, uint32_t &mapped, uint8_t &) override {
        if (addr < 0x8000)
            return false;
        uint8_t total8k = (uint8_t)(prgBanks * 2);
        uint8_t bank;
        if (addr < 0xA000)
            bank = prgReg;
        else if (addr < 0xC000)
            bank = (uint8_t)(total8k - 3);
        else if (addr < 0xE000)
            bank = (uint8_t)(total8k - 2);
        else
            bank = (uint8_t)(total8k - 1);
        mapped = (uint32_t)mapper_helpers::maskBank(bank, total8k) * 0x2000 +
                 (addr & 0x1FFF);
        return true;
    }

    void cpuMapWrite(uint16_t addr, uint32_t &, uint8_t data) override {
        if (addr >= 0x6000 && addr <= 0x7FFF) {
            switch (addr & 0x6003) {
            case 0x6000:
                chr0 = data;
                break;
            case 0x6001:
                chr1 = data;
                break;
            case 0x6002:
                chr2 = data;
                break;
            case 0x6003:
                prgReg = data;
                break;
            }
        }
    }

    bool ppuMapRead(uint16_t addr, uint32_t &mapped) override {
        if (addr > 0x1FFF)
            return false;
        if (chrBanks == 0) {
            mapped = addr;
            return true;
        }
        uint32_t total1k = (uint32_t)chrBanks * 8;
        uint32_t bank1k;
        if (addr < 0x1000) {
            // 4 KB bank: chr0 indexes 4 KB units (bits 0..1 ignored per typical
            // doc).
            uint32_t base4k = ((uint32_t)(chr0 >> 2)) * 4; // in 1KB units
            bank1k = base4k + ((addr >> 10) & 0x03);
        } else if (addr < 0x1800) {
            uint32_t base2k = ((uint32_t)(chr1 >> 1)) * 2;
            bank1k = base2k + ((addr >> 10) & 0x01);
        } else {
            uint32_t base2k = ((uint32_t)(chr2 >> 1)) * 2;
            bank1k = base2k + ((addr >> 10) & 0x01);
        }
        if (total1k)
            bank1k %= total1k;
        mapped = bank1k * 0x0400 + (addr & 0x03FF);
        return true;
    }
    bool ppuMapWrite(uint16_t addr, uint32_t &mapped) override {
        return mapper_helpers::chrRamWrite(addr, mapped, chrBanks);
    }

private:
    uint8_t chr0 = 0, chr1 = 0, chr2 = 0, prgReg = 0;
};

REGISTER_MAPPER(193, Mapper193)
