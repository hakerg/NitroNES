#pragma once
#include "MapperBase.h"
#include "MapperRegistry.h"
#include <array>

// Mapper 064 - Tengen RAMBO-1. Podobny do MMC3 z paroma rozszerzeniami.
// Implementujemy podstawow� logik� bank�w + IRQ (zar�wno tryb scanline jak i
// CPU).
class Mapper064 : public Mapper {
public:
    using Mapper::Mapper;
    void reset() override {
        bankSel = 0;
        chrMode = false;
        prgMode = false;
        k1k = false;
        for (auto &r : R)
            r = 0;
        mirrorMode = Mirroring::HORIZONTAL;
        irqReload = 0;
        irqCounter = 0;
        irqEnable = false;
        irqActive = false;
        irqReloadFlag = false;
        irqCpuMode = false;
        prescaler = 0;
        lastA12 = false;
    }

    bool cpuMapRead(uint16_t a, uint32_t &mapped, uint8_t &) override {
        if (a < 0x8000)
            return false;
        uint32_t total8k = (uint32_t)prgBanks * 2;
        uint8_t bank;
        if (!prgMode) {
            if (a < 0xA000)
                bank = R[6];
            else if (a < 0xC000)
                bank = R[7];
            else if (a < 0xE000)
                bank = R[15];
            else
                bank = (uint8_t)(total8k - 1);
        } else {
            if (a < 0xA000)
                bank = R[15];
            else if (a < 0xC000)
                bank = R[6];
            else if (a < 0xE000)
                bank = R[7];
            else
                bank = (uint8_t)(total8k - 1);
        }
        mapped = (uint32_t)bank * 0x2000 + (a & 0x1FFF);
        return true;
    }

    void cpuMapWrite(uint16_t a, uint32_t &, uint8_t data) override {
        if (a < 0x8000)
            return;
        uint16_t reg = a & 0xE001;
        switch (reg) {
        case 0x8000:
            bankSel = data & 0x0F;
            prgMode = (data & 0x40) != 0;
            chrMode = (data & 0x80) != 0;
            k1k = (data & 0x20) != 0;
            break;
        case 0x8001:
            if (bankSel < 16)
                R[bankSel] = data;
            break;
        case 0xA000:
            mirrorMode =
                (data & 0x01) ? Mirroring::HORIZONTAL : Mirroring::VERTICAL;
            break;
        case 0xC000:
            irqReload = data;
            break;
        case 0xC001:
            irqReloadFlag = true;
            irqCpuMode = (data & 0x01) != 0;
            prescaler = 0;
            break;
        case 0xE000:
            irqEnable = false;
            irqActive = false;
            break;
        case 0xE001:
            irqEnable = true;
            break;
        }
    }

    bool ppuMapRead(uint16_t a, uint32_t &mapped) override {
        if (a > 0x1FFF)
            return false;
        uint8_t bank = 0;
        uint16_t off = a & 0x03FF;
        if (!chrMode) {
            if (a < 0x0800)
                bank = k1k ? ((a < 0x0400) ? R[0] : R[8])
                           : (uint8_t)((R[0] & 0xFE) | ((a >> 10) & 1));
            else if (a < 0x1000)
                bank = k1k ? ((a < 0x0C00) ? R[1] : R[9])
                           : (uint8_t)((R[1] & 0xFE) | ((a >> 10) & 1));
            else if (a < 0x1400)
                bank = R[2];
            else if (a < 0x1800)
                bank = R[3];
            else if (a < 0x1C00)
                bank = R[4];
            else
                bank = R[5];
        } else {
            if (a < 0x0400)
                bank = R[2];
            else if (a < 0x0800)
                bank = R[3];
            else if (a < 0x0C00)
                bank = R[4];
            else if (a < 0x1000)
                bank = R[5];
            else if (a < 0x1800)
                bank = k1k ? ((a < 0x1400) ? R[0] : R[8])
                           : (uint8_t)((R[0] & 0xFE) | ((a >> 10) & 1));
            else
                bank = k1k ? ((a < 0x1C00) ? R[1] : R[9])
                           : (uint8_t)((R[1] & 0xFE) | ((a >> 10) & 1));
        }
        mapped = (uint32_t)bank * 0x0400 + off;
        return true;
    }
    bool ppuMapWrite(uint16_t a, uint32_t &mapped) override {
        return mapper_helpers::chrRamWrite(a, mapped, chrBanks);
    }

    Mirroring mirror() const override { return mirrorMode; }
    bool hasDynamicMirror() const override { return true; }

    void clockA12(uint16_t addr) override {
        const bool a12High = (addr & 0x1000) != 0;
        if (!irqCpuMode && a12High && !lastA12)
            tickIrq();
        lastA12 = a12High;
    }
    void clock() override {
        if (!irqCpuMode)
            return;
        if (++prescaler >= 4) {
            prescaler = 0;
            tickIrq();
        }
    }
    bool irqState() const override { return irqActive; }
    void irqClear() override { irqActive = false; }

private:
    void tickIrq() {
        if (irqReloadFlag) {
            irqCounter = (uint16_t)(irqReload + 1);
            irqReloadFlag = false;
        } else if (irqCounter == 0) {
            irqCounter = irqReload;
        } else {
            irqCounter--;
            if (irqCounter == 0 && irqEnable)
                irqActive = true;
        }
    }

    uint8_t bankSel = 0;
    bool chrMode = false, prgMode = false, k1k = false;
    std::array<uint8_t, 16> R{};
    Mirroring mirrorMode = Mirroring::HORIZONTAL;

    uint8_t irqReload = 0;
    uint16_t irqCounter = 0;
    bool irqEnable = false, irqActive = false, irqReloadFlag = false,
         irqCpuMode = false;
    uint8_t prescaler = 0;
    bool lastA12 = false;
};

REGISTER_MAPPER(64, Mapper064)
