#pragma once
#include "MapperBase.h"
#include "MapperRegistry.h"
#include <array>

// ----------------------------------------------------------------------------
// Mapper 004 - MMC3 / TxROM
// ----------------------------------------------------------------------------
class Mapper004 : public Mapper {
public:
    Mapper004(uint8_t prg, uint8_t chr) : Mapper(prg, chr) {
        pPRGBank.fill(0);
        pCHRBank.fill(0);
        reset();
    }

    void reset() override {
        resetA12();
        targetReg = 0;
        prgMode = false;
        chrInversion = false;
        mirrorMode = Mirroring::VERTICAL;
        irqActive = false;
        irqEnable = false;
        irqReloadFlag = false;
        irqCounter = 0;
        irqReload = 0;
        wramReadOnly = false;
        for (auto &r : pRegister)
            r = 0;
        updateBanks();
    }

    bool cpuMapRead(uint16_t addr, uint32_t &mapped, uint8_t &) override {
        if (addr < 0x8000)
            return false;
        if (addr < 0xA000) {
            mapped = pPRGBank[0] + (addr & 0x1FFF);
            return true;
        }
        if (addr < 0xC000) {
            mapped = pPRGBank[1] + (addr & 0x1FFF);
            return true;
        }
        if (addr < 0xE000) {
            mapped = pPRGBank[2] + (addr & 0x1FFF);
            return true;
        }
        mapped = pPRGBank[3] + (addr & 0x1FFF);
        return true;
    }

    void cpuMapWrite(uint16_t addr, uint32_t &, uint8_t data) override {
        if (addr < 0x8000)
            return;

        if (addr < 0xA000) {
            // $8000-$9FFF
            if ((addr & 1) == 0) {
                // Bank select
                targetReg = data & 0x07;
                prgMode = (data & 0x40) != 0;
                chrInversion = (data & 0x80) != 0;
            } else {
                // Bank data
                pRegister[targetReg] = data;
                updateBanks();
            }
        } else if (addr < 0xC000) {
            // $A000-$BFFF
            if ((addr & 1) == 0) {
                mirrorMode =
                    (data & 0x01) ? Mirroring::HORIZONTAL : Mirroring::VERTICAL;
            } else {
                wramEnabled    = (data & 0x80) != 0;
                wramReadOnly   = (data & 0x40) != 0;
            }
        } else if (addr < 0xE000) {
            // $C000-$DFFF
            if ((addr & 1) == 0) {
                irqReload = data;
            } else {
                irqReloadFlag = true;
                irqCounter = 0;
            }
        } else {
            // $E000-$FFFF
            if ((addr & 1) == 0) {
                irqEnable = false;
                irqActive = false;
            } else {
                irqEnable = true;
            }
        }
    }

    bool cpuReadDirect(uint16_t addr, uint8_t& data) override {
        if (!wramEnabled && addr >= 0x6000 && addr <= 0x7FFF) {
            data = (addr >> 8) & 0xFF;
            return true;
        }
        return false;
    }

    bool cpuWriteDirect(uint16_t addr, uint8_t) override {
        if (!wramEnabled && addr >= 0x6000 && addr <= 0x7FFF)
            return true;
        return wramReadOnly && addr >= 0x6000 && addr <= 0x7FFF;
    }

    bool hasPrgRam() const override { return wramEnabled; }

    bool ppuMapRead(uint16_t addr, uint32_t &mapped) override {
        return chrMapAddr(addr, mapped);
    }
    bool ppuMapWrite(uint16_t addr, uint32_t &mapped) override {
        if (chrBanks > 0) return false;
        return chrMapAddr(addr, mapped);
    }

    Mirroring mirror() const override { return mirrorMode; }
    bool hasDynamicMirror() const override { return true; }

    void ppuAddress(uint16_t addr) override {
        if (!a12RisingEdge(addr)) return;
        bool fire = false;
        if (irqCounter == 0 || irqReloadFlag) {
            // zachowanie rewizji A / MMC6: IRQ tylko przy reloadzie z $C001
            // (flagi reloadu) na wartość 0, nigdy przy "siedzeniu" na 0
            fire = irqReloadFlag && (irqReload == 0);
            irqCounter = irqReload;
            irqReloadFlag = false;
        } else {
            irqCounter--;
            fire = (irqCounter == 0);
        }
        if (fire && irqEnable) {
            irqActive.set(true, 2);
        }
    }

    void clockPpu() override {
        Mapper::clockPpu();
        irqActive.tick();
    }

    bool irqState() const override { return irqActive.get(); }
    void irqClear() override { irqActive = false; }

protected:
    bool chrMapAddr(uint16_t addr, uint32_t &mapped) const {
        if (addr > 0x1FFF)
            return false;
        if (addr < 0x0400)
            mapped = pCHRBank[0] + (addr & 0x03FF);
        else if (addr < 0x0800)
            mapped = pCHRBank[1] + (addr & 0x03FF);
        else if (addr < 0x0C00)
            mapped = pCHRBank[2] + (addr & 0x03FF);
        else if (addr < 0x1000)
            mapped = pCHRBank[3] + (addr & 0x03FF);
        else if (addr < 0x1400)
            mapped = pCHRBank[4] + (addr & 0x03FF);
        else if (addr < 0x1800)
            mapped = pCHRBank[5] + (addr & 0x03FF);
        else if (addr < 0x1C00)
            mapped = pCHRBank[6] + (addr & 0x03FF);
        else
            mapped = pCHRBank[7] + (addr & 0x03FF);
        return true;
    }

    uint8_t chrBankMasked(uint8_t reg) const {
        uint16_t total1k = chrBanks > 0 ? (uint16_t)chrBanks * 8 : chrRam1kBanks;
        if ((total1k & (total1k - 1)) == 0)
            return reg & (total1k - 1);
        return reg % total1k;
    }

    void updateBanks() {
        if (chrInversion) {
            pCHRBank[0] = (uint32_t)chrBankMasked(pRegister[2]) * 0x0400;
            pCHRBank[1] = (uint32_t)chrBankMasked(pRegister[3]) * 0x0400;
            pCHRBank[2] = (uint32_t)chrBankMasked(pRegister[4]) * 0x0400;
            pCHRBank[3] = (uint32_t)chrBankMasked(pRegister[5]) * 0x0400;
            pCHRBank[4] = (uint32_t)chrBankMasked(pRegister[0] & 0xFE) * 0x0400;
            pCHRBank[5] = pCHRBank[4] + 0x0400;
            pCHRBank[6] = (uint32_t)chrBankMasked(pRegister[1] & 0xFE) * 0x0400;
            pCHRBank[7] = pCHRBank[6] + 0x0400;
        } else {
            uint8_t r0 = chrBankMasked(pRegister[0] & 0xFE);
            pCHRBank[0] = (uint32_t)r0 * 0x0400;
            pCHRBank[1] = pCHRBank[0] + 0x0400;
            uint8_t r1 = chrBankMasked(pRegister[1] & 0xFE);
            pCHRBank[2] = (uint32_t)r1 * 0x0400;
            pCHRBank[3] = pCHRBank[2] + 0x0400;
            pCHRBank[4] = (uint32_t)chrBankMasked(pRegister[2]) * 0x0400;
            pCHRBank[5] = (uint32_t)chrBankMasked(pRegister[3]) * 0x0400;
            pCHRBank[6] = (uint32_t)chrBankMasked(pRegister[4]) * 0x0400;
            pCHRBank[7] = (uint32_t)chrBankMasked(pRegister[5]) * 0x0400;
        }

        if (prgMode) {
            pPRGBank[0] = (uint32_t)(prgBanks * 2 - 2) * 0x2000;
            pPRGBank[2] = (uint32_t)mapper_helpers::maskBank(pRegister[6], prgBanks * 2) * 0x2000;
        } else {
            pPRGBank[0] = (uint32_t)mapper_helpers::maskBank(pRegister[6], prgBanks * 2) * 0x2000;
            pPRGBank[2] = (uint32_t)(prgBanks * 2 - 2) * 0x2000;
        }
        pPRGBank[1] = (uint32_t)mapper_helpers::maskBank(pRegister[7], prgBanks * 2) * 0x2000;
        pPRGBank[3] = (uint32_t)(prgBanks * 2 - 1) * 0x2000;
    }

    std::array<uint8_t, 8> pRegister{};
    std::array<uint32_t, 8> pCHRBank{};
    std::array<uint32_t, 4> pPRGBank{};
    uint8_t targetReg = 0;
    bool prgMode = false;
    bool chrInversion = false;
    Mirroring mirrorMode = Mirroring::VERTICAL;

    DelayedPin<bool> irqActive{false};
    bool irqEnable = false, irqReloadFlag = false;
    uint16_t irqCounter = 0, irqReload = 0;
    bool wramEnabled  = true;
    bool wramReadOnly = false;
public:
    std::unique_ptr<Mapper> clone() const override { return std::make_unique<Mapper004>(*this); }
    const char* name() const override { return "MMC3"; }
};

REGISTER_MAPPER(4, Mapper004)
