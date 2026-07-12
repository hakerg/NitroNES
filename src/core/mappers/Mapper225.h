#pragma once
#include "MapperBase.h"
#include "MapperRegistry.h"

// ----------------------------------------------------------------------------
// Mapper 225 — 52-in-1 / 58-in-1 / 64-in-1 multicart
//
// Specyfikacja: nes_specs/mappers/225.txt
//
// Rejestr konfiguracyjny: zapis pod $8000-$FFFF
//   A~[.HMO PPPP PPCC CCCC]
//     bit 13   (H) — MSB wspólny dla PRG i CHR (bit 7 obu rejestrów)
//     bit 12   (M) — mirroring: 0=Vertical, 1=Horizontal
//     bit 11   (O) — tryb PRG: 0=32KB, 1=2×16KB (oba okna na ten sam bank)
//     bity 10-6(P) — bank PRG (16KB jednostki × 2 = 32KB bank)
//     bity 5-0 (C) — bank CHR (8KB jednostki)
//
// RAM $5800-$5803 (4×4 bity, 16 bitów łącznie), mirror $5804-$5FFF.
//
// PRG Mode 0: 32KB bank pod $8000-$FFFF
// PRG Mode 1: 16KB bank pod $8000-$BFFF = $C000-$DFFF (ten sam bank × 2)
// CHR: zawsze 8KB bank pod $0000-$1FFF
// ----------------------------------------------------------------------------
class Mapper225 : public Mapper {
public:
    using Mapper::Mapper;

    void reset() override {
        prgBank = 0;
        chrBank = 0;
        prgMode = false;
        mirror_ = Mirroring::VERTICAL;
        ram[0] = ram[1] = ram[2] = ram[3] = 0;
    }

    // ---- CPU ----------------------------------------------------------------

    bool cpuMapRead(uint16_t addr, uint32_t &mapped, uint8_t &data) override {
        // RAM $5800-$5FFF (4 rejestry, mirror co 4)
        if (addr >= 0x5800 && addr <= 0x5FFF) {
            data = ram[(addr - 0x5800) & 0x03] & 0x0F;
            return true;
        }

        if (addr < 0x8000)
            return false;

        if (prgMode) {
            // PRG Mode 1: oba okna 16KB ($8000-$BFFF i $C000-$FFFF) → ten sam
            // bank
            mapped = (uint32_t)prgBank * 0x4000u + (addr & 0x3FFF);
        } else {
            // PRG Mode 0: 32KB — prgBank jest indeksem 16KB, para banków =
            // prgBank & ~1
            uint32_t base = (uint32_t)(prgBank & ~1u) * 0x4000u;
            mapped = base + (addr & 0x7FFF);
        }
        return true;
    }

    void cpuMapWrite(uint16_t addr, uint32_t & /*mapped*/,
                     uint8_t data) override {
        // RAM $5800-$5FFF
        if (addr >= 0x5800 && addr <= 0x5FFF) {
            ram[(addr - 0x5800) & 0x03] = data & 0x0F;
            return; // nie mapuje na ROM
        }

        // Rejestr konfiguracyjny: dowolny zapis $8000-$FFFF.
        // Wartość na magistrali adresowej (nie danych!) koduje konfigurację:
        //   A~[.HMO PPPP PPCC CCCC]
        //   bit 14 → H  (MSB wspólny dla PRG i CHR)
        //   bit 13 → M  (mirroring)
        //   bit 12 → O  (PRG mode)
        //   bity 11-6 → P (6 bitów banku PRG)
        //   bity  5-0 → C (6 bitów banku CHR)
        if (addr >= 0x8000) {
            uint8_t H = (addr >> 14) & 0x01;
            uint8_t M = (addr >> 13) & 0x01;
            uint8_t O = (addr >> 12) & 0x01;
            uint8_t P = (addr >> 6) & 0x3F; // 6 bitów
            uint8_t C = addr & 0x3F;        // 6 bitów

            prgBank = (uint8_t)((H << 6) | P); // 7-bitowy bank 16KB
            chrBank = (uint8_t)((H << 6) | C); // 7-bitowy bank 8KB
            prgMode = (O == 1);
            mirror_ = (M == 1) ? Mirroring::HORIZONTAL : Mirroring::VERTICAL;
            (void)data;
        }
    }

    // ---- PPU ----------------------------------------------------------------

    bool ppuMapRead(uint16_t addr, uint32_t &mapped) override {
        if (addr <= 0x1FFF) {
            mapped = (uint32_t)chrBank * 0x2000u + addr;
            return true;
        }
        return false;
    }

    bool ppuMapWrite(uint16_t addr, uint32_t &mapped) override {
        // CHR RAM (jeśli ROM nie ma CHR ROM, tzn. chrBanks == 0)
        if (addr <= 0x1FFF && chrBanks == 0) {
            mapped = addr;
            return true;
        }
        return false;
    }

    // ---- Mirroring ----------------------------------------------------------

    Mirroring mirror() const override { return mirror_; }
    bool hasDynamicMirror() const override { return true; }

private:
    uint8_t prgBank = 0;
    uint8_t chrBank = 0;
    bool prgMode = false; // false=32KB, true=2×16KB
    Mirroring mirror_ = Mirroring::VERTICAL;
    uint8_t ram[4] = {}; // $5800-$5803, 4 bity każdy
    const char* name() const override { return "Mapper 225"; }
};

REGISTER_MAPPER(225, Mapper225)
