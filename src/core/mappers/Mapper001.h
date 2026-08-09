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
        prgRamEnabled = true;
        mirrorMode = Mirroring::HORIZONTAL;
    }

    bool cpuMapRead(uint16_t addr, uint32_t &mapped, uint8_t &) override {
        if (addr < 0x6000)
            return false;
        if (addr < 0x8000)
            return false;
        if ((controlReg & 0x08) == 0) {
            uint8_t bank = prgBanks > 16 ? suromPrg32k() : ((prgBankReg & 0x0E) >> 1);
            mapped = (uint32_t)bank * 0x8000 + (addr & 0x7FFF);
        } else {
            uint8_t bank = prgBanks > 16 ? suromPrg16k() : (prgBankReg & 0x0F);
            if (addr < 0xC000) {
                if (controlReg & 0x04)
                    mapped = (uint32_t)bank * 0x4000 + (addr & 0x3FFF);
                else
                    mapped = (uint32_t)suromOuterBank() * 0x4000 + (addr & 0x3FFF);
            } else {
                if (controlReg & 0x04)
                    mapped = (uint32_t)(prgBanks > 16 ? suromOuterBank() | 0x0F : prgBanks - 1) * 0x4000 + (addr & 0x3FFF);
                else
                    mapped = (uint32_t)bank * 0x4000 + (addr & 0x3FFF);
            }
        }
        return true;
    }

    void cpuMapWrite(uint16_t addr, uint32_t &mapped, uint8_t data) override {
        if (addr < 0x8000)
            return;

        if (data & 0x80) {
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
                if (chrBanks == 0 && prgBanks <= 16)
                    prgRamEnabled = !(loadReg & 0x10);
                break;
            case 2:
                chrBank1 = loadReg & 0x1F;
                break;
            case 3:
                prgBankReg = loadReg & 0x1F;
                prgRamEnabled = !(prgBankReg & 0x10);
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
        if (chrBanks == 0)
            return chrMapAddr(addr, mapped);

        if ((controlReg & 0x10) == 0) {
            uint8_t bank = mapper_helpers::maskBank((chrBank0 & 0x1E) >> 1, chrBanks);
            mapped = (uint32_t)bank * 0x2000 + (addr & 0x1FFF);
        } else {
            if (addr < 0x1000)
                mapped = (uint32_t)chrBankMasked4k(chrBank0) * 0x1000 + (addr & 0x0FFF);
            else
                mapped = (uint32_t)chrBankMasked4k(chrBank1) * 0x1000 + (addr & 0x0FFF);
        }
        return true;
    }
    bool ppuMapWrite(uint16_t addr, uint32_t &mapped) override {
        if (chrBanks > 0) return false;
        return chrMapAddr(addr, mapped);
    }

    Mirroring mirror() const override { return mirrorMode; }
    bool hasDynamicMirror() const override { return true; }
    bool hasPrgRam() const override { return prgRamEnabled; }

private:
    uint8_t suromOuterBank() const {
        return prgBanks > 16 ? chrBank0 & 0x10 : 0;
    }
    uint8_t suromPrg16k() const {
        return suromOuterBank() | (prgBankReg & 0x0F);
    }
    uint8_t suromPrg32k() const {
        return suromPrg16k() >> 1;
    }

    bool chrMapAddr(uint16_t addr, uint32_t &mapped) const {
        if (addr > 0x1FFF)
            return false;
        if (chrBanks == 0) {
            if (controlReg & 0x10)
                mapped = (addr < 0x1000 ? chrBankMasked4k(chrBank0) : chrBankMasked4k(chrBank1)) * 0x1000 + (addr & 0x0FFF);
            else
                mapped = addr;
            return true;
        }
        if ((controlReg & 0x10) == 0) {
            uint8_t bank = mapper_helpers::maskBank((chrBank0 & 0x1E) >> 1, chrBanks);
            mapped = (uint32_t)bank * 0x2000 + (addr & 0x1FFF);
        } else {
            if (addr < 0x1000)
                mapped = (uint32_t)chrBankMasked4k(chrBank0) * 0x1000 + (addr & 0x0FFF);
            else
                mapped = (uint32_t)chrBankMasked4k(chrBank1) * 0x1000 + (addr & 0x0FFF);
        }
        return true;
    }

    uint8_t chrBankMasked4k(uint8_t reg) const {
        uint16_t total = chrBanks > 0 ? (uint16_t)chrBanks * 2 : chrRam1kBanks / 4;
        if (total == 0) return reg;
        if ((total & (total - 1)) == 0)
            return reg & (total - 1);
        return reg % total;
    }

    uint8_t loadReg = 0, loadCount = 0;
    uint8_t controlReg = 0x1C;
    uint8_t chrBank0 = 0, chrBank1 = 0, prgBankReg = 0;
    bool prgRamEnabled = true;
    Mirroring mirrorMode = Mirroring::HORIZONTAL;
public:
    std::unique_ptr<Mapper> clone() const override { return std::make_unique<Mapper001>(*this); }
    const char* name() const override { return "MMC1"; }
};

REGISTER_MAPPER(1, Mapper001)
