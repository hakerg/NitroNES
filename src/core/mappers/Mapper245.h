#pragma once
#include "MapperBase.h"
#include "MapperRegistry.h"
#include <array>

// ----------------------------------------------------------------------------
// Mapper 245 - MMC3 clone (chinski) z PRG-OR sterowanym przez R:0 bit 1
// ----------------------------------------------------------------------------
// Identyczny jak MMC3, ale:
//   * PRG-AND = $3F, PRG-OR = (R:0 bit 1) ? $40 : $00
//   * CHR-RAM nie jest swappowalna; gdy chrBanks==0, bit chrInversion z $8000
//     dalej moze "flipowac" lewa/prawa polowke pattern table (banki 0 i 1).
// ----------------------------------------------------------------------------
class Mapper245 : public Mapper {
public:
    Mapper245(uint8_t prg, uint8_t chr) : Mapper(prg, chr) {
        pCHRBank.fill(0);
        reset();
    }

    void reset() override {
        resetA12();
        targetReg = 0;
        prgMode = false;
        chrInversion = false;
        mirrorMode = Mirroring::HORIZONTAL;
        irqActive = false;
        irqEnable = false;
        irqReloadFlag = false;
        irqCounter = 0;
        irqReload = 0;
        for (auto &r : pRegister)
            r = 0;
        pCHRBank.fill(0);
    }

    bool cpuMapRead(uint16_t addr, uint32_t &mapped, uint8_t &) override {
        if (addr < 0x8000)
            return false;
        uint8_t prgOr = (pRegister[0] & 0x02) ? 0x40 : 0x00;
        uint8_t prgAnd = 0x3F;
        uint8_t last = (uint8_t)((prgBanks * 2 - 1) & prgAnd) | prgOr;
        uint8_t prev = (uint8_t)((prgBanks * 2 - 2) & prgAnd) | prgOr;
        uint8_t r6 = (uint8_t)((pRegister[6] & prgAnd) | prgOr);
        uint8_t r7 = (uint8_t)((pRegister[7] & prgAnd) | prgOr);
        uint8_t bank;
        if (addr < 0xA000)
            bank = prgMode ? prev : r6;
        else if (addr < 0xC000)
            bank = r7;
        else if (addr < 0xE000)
            bank = prgMode ? r6 : prev;
        else
            bank = last;
        mapped = (uint32_t)bank * 0x2000 + (addr & 0x1FFF);
        return true;
    }

    void cpuMapWrite(uint16_t addr, uint32_t &, uint8_t data) override {
        if (addr < 0x8000)
            return;
        if (addr < 0xA000) {
            if ((addr & 1) == 0) {
                targetReg = data & 0x07;
                prgMode = (data & 0x40) != 0;
                chrInversion = (data & 0x80) != 0;
            } else {
                pRegister[targetReg] = data;
            }
        } else if (addr < 0xC000) {
            if ((addr & 1) == 0)
                mirrorMode =
                    (data & 0x01) ? Mirroring::HORIZONTAL : Mirroring::VERTICAL;
        } else if (addr < 0xE000) {
            if ((addr & 1) == 0)
                irqReload = data;
            else {
                irqReloadFlag = true;
                irqCounter = 0;
            }
        } else {
            if ((addr & 1) == 0) {
                irqEnable = false;
                irqActive = false;
            } else
                irqEnable = true;
        }
    }

    bool ppuMapRead(uint16_t addr, uint32_t &mapped) override {
        if (addr > 0x1FFF)
            return false;
        if (chrBanks == 0) {
            // CHR-RAM: tylko 'flip' przez chrInversion
            if (chrInversion)
                mapped = (uint32_t)((addr ^ 0x1000) & 0x1FFF);
            else
                mapped = addr;
            return true;
        }
        uint8_t slot = (uint8_t)(addr >> 10);
        if (chrInversion) {
            static const uint8_t map[8] = {2, 3, 4, 5, 0, 0, 1, 1};
            uint8_t r = pRegister[map[slot]];
            if (slot >= 4) {
                r = (uint8_t)((r & 0xFE) + (slot & 1));
            }
            mapped = (uint32_t)r * 0x0400 + (addr & 0x03FF);
        } else {
            static const uint8_t map[8] = {0, 0, 1, 1, 2, 3, 4, 5};
            uint8_t r = pRegister[map[slot]];
            if (slot < 4) {
                r = (uint8_t)((r & 0xFE) + (slot & 1));
            }
            mapped = (uint32_t)r * 0x0400 + (addr & 0x03FF);
        }
        return true;
    }
    bool ppuMapWrite(uint16_t addr, uint32_t &mapped) override {
        if (chrBanks == 0 && addr <= 0x1FFF) {
            if (chrInversion)
                mapped = (uint32_t)((addr ^ 0x1000) & 0x1FFF);
            else
                mapped = addr;
            return true;
        }
        return false;
    }

    Mirroring mirror() const override { return mirrorMode; }
    bool hasDynamicMirror() const override { return true; }

    void ppuAddress(uint16_t addr) override {
        if (a12RisingEdge(addr)) tickIrq();
    }
    bool irqState() const override { return irqActive; }
    void irqClear() override { irqActive = false; }

private:
    void tickIrq() {
        if (irqCounter == 0 || irqReloadFlag) {
            irqCounter = irqReload;
            irqReloadFlag = false;
        } else
            irqCounter--;
        if (irqCounter == 0 && irqEnable)
            irqActive = true;
    }

    std::array<uint8_t, 8> pRegister{};
    std::array<uint32_t, 8> pCHRBank{};
    uint8_t targetReg = 0;
    bool prgMode = false, chrInversion = false;
    Mirroring mirrorMode = Mirroring::HORIZONTAL;
    bool irqActive = false, irqEnable = false, irqReloadFlag = false;
    uint16_t irqCounter = 0, irqReload = 0;
    const char* name() const override { return "MMC3 clone (chinski) z PRG-OR sterowanym przez R:0 bit 1"; }
};

REGISTER_MAPPER(245, Mapper245)
