#pragma once
#include "MapperBase.h"
#include "MapperRegistry.h"

// ----------------------------------------------------------------------------
// Mapper 001 - MMC1 / SxROM
// ----------------------------------------------------------------------------
class Mapper001 : public Mapper {
public:
    Mapper001(uint8_t prg, uint8_t chr) : Mapper(prg, chr) { reset(); }

    void reset() override {
        loadReg = 0x00;
        loadCount = 0;
        controlReg =
            0x1C; // 16k PRG, $8000 swap, 1-screen lo (bity 2,3 ustawione)
        chrBank0 = chrBank1 = prgBankReg = 0;
        mirrorMode = Mirroring::HORIZONTAL;
    }

    bool cpuMapRead(uint16_t addr, uint32_t &mapped, uint8_t &) override {
        if (addr < 0x6000)
            return false;
        if (addr < 0x8000) {
            // 8KB PRG-RAM (obsluzona w Cartridge), tu sygnalizujemy ze to nie
            // ROM
            return false;
        }
        if ((controlReg & 0x08) == 0) {
            // 32KB mode
            uint8_t bank = (prgBankReg & 0x0E) >> 1;
            mapped = (uint32_t)bank * 0x8000 + (addr & 0x7FFF);
        } else {
            // 16KB mode
            uint8_t bank = prgBankReg & 0x0F;
            if (addr < 0xC000) {
                if (controlReg & 0x04) {
                    mapped = (uint32_t)bank * 0x4000 + (addr & 0x3FFF);
                } else {
                    mapped = 0x0000 + (addr & 0x3FFF);
                }
            } else {
                if (controlReg & 0x04) {
                    mapped =
                        (uint32_t)(prgBanks - 1) * 0x4000 + (addr & 0x3FFF);
                } else {
                    mapped = (uint32_t)bank * 0x4000 + (addr & 0x3FFF);
                }
            }
        }
        return true;
    }

    void cpuMapWrite(uint16_t addr, uint32_t &mapped, uint8_t data) override {
        if (addr < 0x8000)
            return;

        if (data & 0x80) {
            // Reset wewnetrznego latcha. Wg specyfikacji MMC1: gdy bit 'r' jest
            // ustawiony, bit 'd' jest IGNOROWANY i sekwencja 5-bitowa zaczyna
            // sie od poczatku. Dodatkowo bity 2,3 controlReg sa ustawiane
            // (16k PRG, $8000 swappable). Bez 'return' kolejny zapis trafialby
            // jako 1. bit do loadReg, a loadCount startowalby od 1 - efekt:
            // rejestry MMC1 nigdy nie zatrzaskuja sie we wlasciwym miejscu i
            // gra (np. testy Blargga z MMC1) zawiesza sie w petli
            // inicjalizacji.
            loadReg = 0x00;
            loadCount = 0;
            controlReg |= 0x0C;
            (void)mapped;
            return;
        }

        loadReg >>= 1;
        loadReg |= (data & 0x01) << 4;
        loadCount++;

        if (loadCount == 5) {
            uint8_t targetReg =
                (addr >> 13) &
                0x03; // 0:8000-9FFF 1:A000-BFFF 2:C000-DFFF 3:E000-FFFF
            switch (targetReg) {
            case 0:
                controlReg = loadReg & 0x1F;
                switch (controlReg & 0x03) {
                case 0:
                    mirrorMode = Mirroring::ONESCREEN_LO;
                    break;
                case 1:
                    mirrorMode = Mirroring::ONESCREEN_HI;
                    break;
                case 2:
                    mirrorMode = Mirroring::VERTICAL;
                    break;
                case 3:
                    mirrorMode = Mirroring::HORIZONTAL;
                    break;
                }
                break;
            case 1:
                chrBank0 = loadReg & 0x1F;
                break;
            case 2:
                chrBank1 = loadReg & 0x1F;
                break;
            case 3:
                prgBankReg = loadReg & 0x1F;
                break;
            }
            loadReg = 0;
            loadCount = 0;
        }
        (void)mapped;
    }

    bool ppuMapRead(uint16_t addr, uint32_t &mapped) override {
        if (addr > 0x1FFF)
            return false;
        if (chrBanks == 0) {
            mapped = addr;
            return true;
        }

        if ((controlReg & 0x10) == 0) {
            // 8KB - dolny bit chrBank0 jest ignorowany (wybor 8KB pary)
            uint8_t bank = (chrBank0 & 0x1E) >> 1;
            mapped = (uint32_t)bank * 0x2000 + (addr & 0x1FFF);
        } else {
            // 4KB
            if (addr < 0x1000) {
                mapped = (uint32_t)chrBank0 * 0x1000 + (addr & 0x0FFF);
            } else {
                mapped = (uint32_t)chrBank1 * 0x1000 + (addr & 0x0FFF);
            }
        }
        return true;
    }
    bool ppuMapWrite(uint16_t addr, uint32_t &mapped) override {
        return mapper_helpers::chrRamWrite(addr, mapped, chrBanks);
    }

    Mirroring mirror() const override { return mirrorMode; }
    bool hasDynamicMirror() const override { return true; }

private:
    uint8_t loadReg = 0, loadCount = 0;
    uint8_t controlReg = 0x1C;
    uint8_t chrBank0 = 0, chrBank1 = 0, prgBankReg = 0;
    Mirroring mirrorMode = Mirroring::HORIZONTAL;
};

REGISTER_MAPPER(1, Mapper001)
