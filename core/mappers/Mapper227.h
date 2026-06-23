#pragma once
#include "MapperBase.h"
#include "MapperRegistry.h"

// ----------------------------------------------------------------------------
// Mapper 227 - 1200-in-1
// ----------------------------------------------------------------------------
// $8000-FFFF: A~[.... ..LP  OPPP PPMS]
//   L = last PRG page mode
//   P = PRG reg (6 bitow)
//   O = mode (1: dwie polowy ta sama P; 0: $8000 z P, $C000 modyfikowane)
//   M = mirroring (0=V, 1=H)
//   S = PRG size (0=16K, 1=32K)
// CHR-RAM 8 KB; przy O=1 chronione (write disable).
// ----------------------------------------------------------------------------
class Mapper227 : public Mapper {
public:
    using Mapper::Mapper;

    void reset() override {
        P = 0;
        O = false;
        L = false;
        S = false;
        mirrorMode = Mirroring::VERTICAL;
    }

    bool cpuMapRead(uint16_t addr, uint32_t &mapped, uint8_t &) override {
        if (addr < 0x8000)
            return false;
        uint8_t lo, hi;
        if (O) {
            if (S) { // 32K mode
                uint8_t n = prgBanks / 2;
                if (n == 0)
                    n = 1;
                uint8_t b = mapper_helpers::maskBank(P >> 1, n);
                mapped = (uint32_t)b * 0x8000 + (addr & 0x7FFF);
                return true;
            }
            lo = P;
            hi = P;
        } else {
            lo = S ? (uint8_t)(P & 0x3E) : P;
            hi = L ? (uint8_t)(P | 0x07) : (uint8_t)(P & 0x38);
        }
        uint8_t bank = (addr < 0xC000) ? lo : hi;
        bank = mapper_helpers::maskBank(bank, prgBanks);
        mapped = (uint32_t)bank * 0x4000 + (addr & 0x3FFF);
        return true;
    }

    void cpuMapWrite(uint16_t addr, uint32_t &, uint8_t /*data*/) override {
        if (addr < 0x8000)
            return;
        uint16_t a = addr;
        P = (uint8_t)(((a >> 2) & 0x1F) |
                      (((a >> 8) & 0x01) << 5)); // 5 + 1 = 6 bity
        S = (a & 0x01) != 0;
        mirrorMode = (a & 0x02) ? Mirroring::HORIZONTAL : Mirroring::VERTICAL;
        O = (a & 0x80) != 0;
        L = (a & 0x200) != 0;
    }

    bool ppuMapRead(uint16_t addr, uint32_t &mapped) override {
        if (addr > 0x1FFF)
            return false;
        mapped = addr;
        return true;
    }
    bool ppuMapWrite(uint16_t addr, uint32_t &mapped) override {
        if (O)
            return false; // CHR-RAM write protect
        return mapper_helpers::chrRamWrite(addr, mapped, chrBanks);
    }
    Mirroring mirror() const override { return mirrorMode; }
    bool hasDynamicMirror() const override { return true; }

private:
    uint8_t P = 0;
    bool O = false, L = false, S = false;
    Mirroring mirrorMode = Mirroring::VERTICAL;
};

REGISTER_MAPPER(227, Mapper227)
