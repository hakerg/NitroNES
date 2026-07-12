#pragma once
#include "MapperBase.h"
#include "MapperRegistry.h"
#include <array>

// ----------------------------------------------------------------------------
// Mapper 048 - Taito TC0190FMC + PAL16R4 (z IRQ)
// ----------------------------------------------------------------------------
// Bardzo zblizony do MMC3 (mapper 004). Roznice:
//   - rejestry maja wlasny uklad w $8000-$FFFF (mask $E003)
//   - IRQ dziala jak w MMC3, ale reload jest XOR-owany z $FF
//   - mirroring sterowany bitem 6 zapisu pod $E000
//   - brak rejestru "bank select" - kazdy adres adresuje konkretny rejestr
// ----------------------------------------------------------------------------
class Mapper048 : public Mapper {
public:
    Mapper048(uint8_t prg, uint8_t chr) : Mapper(prg, chr) { reset(); }

    void reset() override {
        resetA12();
        prg0 = 0;
        prg1 = 1;
        chr0 = chr1 = 0;
        chr2 = chr3 = chr4 = chr5 = 0;
        mirrorMode = Mirroring::HORIZONTAL;
        irqActive = irqEnable = irqReloadFlag = false;
        irqCounter = irqReload = 0;
    }

    bool cpuMapRead(uint16_t addr, uint32_t &mapped, uint8_t &) override {
        if (addr < 0x8000)
            return false;
        uint8_t total8k = (uint8_t)(prgBanks * 2);
        uint8_t bank;
        if (addr < 0xA000)
            bank = prg0;
        else if (addr < 0xC000)
            bank = prg1;
        else if (addr < 0xE000)
            bank = (uint8_t)(total8k - 2);
        else
            bank = (uint8_t)(total8k - 1);
        mapped = (uint32_t)mapper_helpers::maskBank(bank, total8k) * 0x2000 +
                 (addr & 0x1FFF);
        return true;
    }

    void cpuMapWrite(uint16_t addr, uint32_t &, uint8_t data) override {
        if (addr < 0x8000)
            return;
        uint16_t reg = addr & 0xE003;
        switch (reg) {
        case 0x8000:
            prg0 = data & 0x3F;
            break;
        case 0x8001:
            prg1 = data & 0x3F;
            break;
        case 0x8002:
            chr0 = data;
            break;
        case 0x8003:
            chr1 = data;
            break;
        case 0xA000:
            chr2 = data;
            break;
        case 0xA001:
            chr3 = data;
            break;
        case 0xA002:
            chr4 = data;
            break;
        case 0xA003:
            chr5 = data;
            break;
        case 0xC000:
            irqReload = (uint8_t)(data ^ 0xFF);
            break;
        case 0xC001:
            irqReloadFlag = true;
            irqCounter = 0;
            break;
        case 0xC002:
            irqEnable = true;
            break;
        case 0xC003:
            irqEnable = false;
            irqActive = false;
            break;
        case 0xE000:
            mirrorMode =
                (data & 0x40) ? Mirroring::HORIZONTAL : Mirroring::VERTICAL;
            break;
        default:
            break;
        }
    }

    bool ppuMapRead(uint16_t addr, uint32_t &mapped) override {
        if (addr > 0x1FFF)
            return false;
        uint32_t total1k = (uint32_t)chrBanks * 8;
        uint32_t bank1k;
        if (addr < 0x0800) {
            uint32_t base = ((uint32_t)(chr0 >> 1)) * 2;
            bank1k = base + ((addr >> 10) & 0x01);
        } else if (addr < 0x1000) {
            uint32_t base = ((uint32_t)(chr1 >> 1)) * 2;
            bank1k = base + ((addr >> 10) & 0x01);
        } else if (addr < 0x1400)
            bank1k = chr2;
        else if (addr < 0x1800)
            bank1k = chr3;
        else if (addr < 0x1C00)
            bank1k = chr4;
        else
            bank1k = chr5;
        if (total1k)
            bank1k %= total1k;
        mapped = bank1k * 0x0400 + (addr & 0x03FF);
        return true;
    }
    bool ppuMapWrite(uint16_t addr, uint32_t &mapped) override {
        return mapper_helpers::chrRamWrite(addr, mapped, chrBanks);
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

    uint8_t prg0 = 0, prg1 = 0;
    uint8_t chr0 = 0, chr1 = 0, chr2 = 0, chr3 = 0, chr4 = 0, chr5 = 0;
    Mirroring mirrorMode = Mirroring::HORIZONTAL;
    bool irqActive = false, irqEnable = false, irqReloadFlag = false;
    uint16_t irqCounter = 0, irqReload = 0;
    const char* name() const override { return "Taito TC0190FMC + PAL16R4 (z IRQ)"; }
};

REGISTER_MAPPER(48, Mapper048)
