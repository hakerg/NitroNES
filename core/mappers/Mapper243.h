#pragma once
#include "MapperBase.h"
#include "MapperRegistry.h"

// ----------------------------------------------------------------------------
// Mapper 243 - Sachen (Honey, Poker III 5-in-1)
// ----------------------------------------------------------------------------
// Range,Mask: $4020-$4FFF, $4101
//   $4100: addr port [.... .AAA]
//   $4101: data port
//     R:2 -> high bit CHR
//     R:4 -> low  bit CHR
//     R:5 -> PRG (32K)
//     R:6 -> middle 2 bits CHR
//     R:7 -> mirror (00=H, 01=V, 10=specjalne, 11=1ScB)
// CHR (4 bity) = (H << 3) | (D << 1) | L = "HDDL"
// ----------------------------------------------------------------------------
class Mapper243 : public Mapper {
public:
    using Mapper::Mapper;

    void reset() override {
        addrPort = 0;
        prg = 0;
        chrH = chrL = 0;
        chrD = 0;
        mirrorMode = Mirroring::HORIZONTAL;
    }

    bool cpuMapRead(uint16_t addr, uint32_t &mapped, uint8_t &) override {
        if (addr < 0x8000)
            return false;
        mapped = mapper_helpers::mapPrg32k(addr, prg, prgBanks);
        return true;
    }

    void cpuMapWrite(uint16_t addr, uint32_t &, uint8_t data) override {
        if (addr < 0x4020 || addr >= 0x5000)
            return;
        if ((addr & 0x4101) == 0x4100) {
            addrPort = data & 0x07;
        } else if ((addr & 0x4101) == 0x4101) {
            switch (addrPort) {
            case 2:
                chrH = data & 0x01;
                break;
            case 4:
                chrL = data & 0x01;
                break;
            case 5:
                prg = data & 0x07;
                break;
            case 6:
                chrD = data & 0x03;
                break;
            case 7: {
                uint8_t m = (data >> 1) & 0x03;
                switch (m) {
                case 0:
                    mirrorMode = Mirroring::HORIZONTAL;
                    break;
                case 1:
                    mirrorMode = Mirroring::VERTICAL;
                    break;
                case 2:
                    mirrorMode = Mirroring::ONESCREEN_LO;
                    break; // przyblizenie
                case 3:
                    mirrorMode = Mirroring::ONESCREEN_HI;
                    break;
                }
                break;
            }
            default:
                break;
            }
        }
    }

    bool ppuMapRead(uint16_t addr, uint32_t &mapped) override {
        if (addr > 0x1FFF)
            return false;
        uint8_t bank = (uint8_t)((chrH << 3) | (chrD << 1) | chrL);
        mapped = mapper_helpers::mapChr8k(addr, bank, chrBanks);
        return true;
    }
    bool ppuMapWrite(uint16_t addr, uint32_t &mapped) override {
        return mapper_helpers::chrRamWrite(addr, mapped, chrBanks);
    }
    Mirroring mirror() const override { return mirrorMode; }
    bool hasDynamicMirror() const override { return true; }

private:
    uint8_t addrPort = 0;
    uint8_t prg = 0, chrH = 0, chrL = 0, chrD = 0;
    Mirroring mirrorMode = Mirroring::HORIZONTAL;
};

REGISTER_MAPPER(243, Mapper243)
