#pragma once
#include "MapperBase.h"
#include "MapperRegistry.h"
#include <array>

// ----------------------------------------------------------------------------
// Mapper 189 - MMC3 modified (Thunder Warrior)
// ----------------------------------------------------------------------------
// Identyczny jak MMC3 (mapper 004), ale R:6/R:7 sa ignorowane. Zamiast tego
// pojedynczy 32 KB PRG bank wybierany przez zapis do $4120-$7FFF
// (bity 7..4 i 3..0 sa OR'owane razem - kazdy nibble moze zawierac bank).
// CHR, mirroring i IRQ - jak w MMC3.
// ----------------------------------------------------------------------------
class Mapper189 : public Mapper {
public:
    Mapper189(uint8_t prg, uint8_t chr) : Mapper(prg, chr) {
        pCHRBank.fill(0);
        reset();
    }

    void reset() override {
        targetReg = 0;
        prgMode = false;
        chrInversion = false;
        mirrorMode = Mirroring::HORIZONTAL;
        irqActive = false;
        irqEnable = false;
        irqReloadFlag = false;
        irqCounter = 0;
        irqReload = 0;
        lastA12 = false;
        for (auto &r : pRegister)
            r = 0;
        pCHRBank.fill(0);
        prg32 = 0;
    }

    bool cpuMapRead(uint16_t addr, uint32_t &mapped, uint8_t &) override {
        if (addr < 0x8000)
            return false;
        // 32 KB switchable bank dla calego $8000-FFFF
        uint8_t n = prgBanks / 2;
        if (n == 0)
            n = 1;
        uint8_t bank = mapper_helpers::maskBank(prg32, n);
        mapped = (uint32_t)bank * 0x8000 + (addr & 0x7FFF);
        return true;
    }

    void cpuMapWrite(uint16_t addr, uint32_t &, uint8_t data) override {
        if (addr >= 0x4120 && addr < 0x8000) {
            prg32 = ((data >> 4) & 0x0F) | (data & 0x0F);
            return;
        }
        if (addr < 0x8000)
            return;

        if (addr < 0xA000) {
            if ((addr & 1) == 0) {
                targetReg = data & 0x07;
                prgMode = (data & 0x40) != 0;
                chrInversion = (data & 0x80) != 0;
            } else {
                pRegister[targetReg] = data;
                updateChr();
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
        uint8_t slot = (uint8_t)(addr >> 10);
        mapped = pCHRBank[slot] + (addr & 0x03FF);
        return true;
    }
    bool ppuMapWrite(uint16_t addr, uint32_t &mapped) override {
        return mapper_helpers::chrRamWrite(addr, mapped, chrBanks);
    }

    Mirroring mirror() const override { return mirrorMode; }
    bool hasDynamicMirror() const override { return true; }

    void scanline() override { tickIrq(); }
    void clockA12(uint16_t addr) override {
        const bool a12High = (addr & 0x1000) != 0;
        if (a12High && !lastA12)
            tickIrq();
        lastA12 = a12High;
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

    void updateChr() {
        if (chrInversion) {
            pCHRBank[0] = (uint32_t)pRegister[2] * 0x0400;
            pCHRBank[1] = (uint32_t)pRegister[3] * 0x0400;
            pCHRBank[2] = (uint32_t)pRegister[4] * 0x0400;
            pCHRBank[3] = (uint32_t)pRegister[5] * 0x0400;
            pCHRBank[4] = (uint32_t)(pRegister[0] & 0xFE) * 0x0400;
            pCHRBank[5] = pCHRBank[4] + 0x0400;
            pCHRBank[6] = (uint32_t)(pRegister[1] & 0xFE) * 0x0400;
            pCHRBank[7] = pCHRBank[6] + 0x0400;
        } else {
            pCHRBank[0] = (uint32_t)(pRegister[0] & 0xFE) * 0x0400;
            pCHRBank[1] = pCHRBank[0] + 0x0400;
            pCHRBank[2] = (uint32_t)(pRegister[1] & 0xFE) * 0x0400;
            pCHRBank[3] = pCHRBank[2] + 0x0400;
            pCHRBank[4] = (uint32_t)pRegister[2] * 0x0400;
            pCHRBank[5] = (uint32_t)pRegister[3] * 0x0400;
            pCHRBank[6] = (uint32_t)pRegister[4] * 0x0400;
            pCHRBank[7] = (uint32_t)pRegister[5] * 0x0400;
        }
    }

    std::array<uint8_t, 8> pRegister{};
    std::array<uint32_t, 8> pCHRBank{};
    uint8_t targetReg = 0, prg32 = 0;
    bool prgMode = false, chrInversion = false;
    Mirroring mirrorMode = Mirroring::HORIZONTAL;
    bool irqActive = false, irqEnable = false, irqReloadFlag = false,
         lastA12 = false;
    uint16_t irqCounter = 0, irqReload = 0;
};

REGISTER_MAPPER(189, Mapper189)
