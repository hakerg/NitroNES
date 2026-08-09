#pragma once
#include "MapperBase.h"
#include "MapperRegistry.h"

// ----------------------------------------------------------------------------
// Mapper 228 - Action 52 / Cheetah Men II
// ----------------------------------------------------------------------------
// $8000-FFFF:  data=[......CC] (low 2 bits CHR),
//              addr=A~[..MH HPPP PPO. CCCC]
//   M = mirror (0=V, 1=H)
//   H = chip select (0,1,2(open bus),3)
//   P = PRG page (5 bits) within current chip
//   O = PRG mode (0=32K, 1=16K mirrored)
//   C = high 4 bits CHR (8 KB pages -> total 6 bits)
//
// Uproszczenie: traktujemy PRG jako liniowe (bez special-casingu chip 2 open
// bus). Reordered chip map (0,1,2,3) -> w pliku iNES kolejne 512 KB regionow.
// ----------------------------------------------------------------------------
class Mapper228 : public Mapper {
public:
    using Mapper::Mapper;

    void reset() override {
        prgChip = 0;
        prgPage = 0;
        mode16 = false;
        chrLo = 0;
        chrHi = 0;
        mirrorMode = Mirroring::VERTICAL;
    }

    bool cpuMapRead(uint16_t addr, uint32_t &mapped, uint8_t &) override {
        if (addr < 0x8000)
            return false;
        uint8_t chip = prgChip;
        if (chip == 2)
            chip =
                1; // chip 2 nie istnieje - mapujemy na chip 1 zamiast open bus
        uint32_t chipBase = (uint32_t)chip * (512 * 1024);
        uint32_t off;
        if (mode16) {
            off = chipBase + (uint32_t)prgPage * 0x4000 + (addr & 0x3FFF);
        } else {
            off =
                chipBase + (uint32_t)(prgPage >> 1) * 0x8000 + (addr & 0x7FFF);
        }
        uint32_t romSize = (uint32_t)prgBanks * 0x4000;
        if (romSize)
            off %= romSize;
        mapped = off;
        return true;
    }

    void cpuMapWrite(uint16_t addr, uint32_t &, uint8_t data) override {
        if (addr < 0x8000)
            return;
        uint16_t a = addr;
        mirrorMode = (a & 0x2000) ? Mirroring::HORIZONTAL : Mirroring::VERTICAL;
        prgChip = (uint8_t)((a >> 11) & 0x03);
        prgPage = (uint8_t)((a >> 6) & 0x1F);
        mode16 = (a & 0x20) != 0;
        chrLo = data & 0x03;
        chrHi = (uint8_t)((a >> 0) & 0x0F); // 4 niskie bity adresu
    }

    bool ppuMapRead(uint16_t addr, uint32_t &mapped) override {
        if (addr > 0x1FFF)
            return false;
        uint8_t bank = (uint8_t)((chrHi << 2) | chrLo);
        mapped = mapper_helpers::mapChr8k(addr, bank, chrBanks);
        return true;
    }
    bool ppuMapWrite(uint16_t addr, uint32_t &mapped) override {
        return mapper_helpers::chrRamWrite(addr, mapped, chrBanks);
    }
    Mirroring mirror() const override { return mirrorMode; }
    bool hasDynamicMirror() const override { return true; }

private:
    uint8_t prgChip = 0, prgPage = 0;
    bool mode16 = false;
    uint8_t chrLo = 0, chrHi = 0;
    Mirroring mirrorMode = Mirroring::VERTICAL;
public:
    std::unique_ptr<Mapper> clone() const override { return std::make_unique<Mapper228>(*this); }
    const char* name() const override { return "Action 52 / Cheetah Men II"; }
};

REGISTER_MAPPER(228, Mapper228)
