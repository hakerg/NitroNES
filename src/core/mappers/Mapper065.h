#pragma once
#include "MapperBase.h"
#include "MapperRegistry.h"
#include <array>

// Mapper 065 - Irem H3001. 16-bit licznik IRQ (CPU cycle) z reloadem.
class Mapper065 : public Mapper {
public:
    using Mapper::Mapper;
    void reset() override {
        prg[0] = 0;
        prg[1] = 1;
        // $C000 init = $FE (zgodnie z nes_specs)
        prg[2] = (uint8_t)(prgBanks * 2 - 2);
        for (auto &c : chr)
            c = 0;
        mirrorMode = Mirroring::VERTICAL;
        irqEnable = false;
        irqActive = false;
        irqReload = 0;
        irqCounter = 0;
    }

    bool cpuMapRead(uint16_t a, uint32_t &mapped, uint8_t &) override {
        if (a < 0x8000)
            return false;
        uint32_t total8k = (uint32_t)prgBanks * 2;
        uint8_t b;
        if (a < 0xA000)
            b = prg[0];
        else if (a < 0xC000)
            b = prg[1];
        else if (a < 0xE000)
            b = prg[2];
        else
            b = (uint8_t)(total8k - 1);
        mapped = (uint32_t)b * 0x2000 + (a & 0x1FFF);
        return true;
    }
    void cpuMapWrite(uint16_t a, uint32_t &, uint8_t data) override {
        if (a < 0x8000)
            return;
        switch (a) {
        case 0x8000:
            prg[0] = data;
            break;
        case 0xA000:
            prg[1] = data;
            break;
        case 0xC000:
            prg[2] = data;
            break;
        case 0x9001:
            mirrorMode =
                (data & 0x80) ? Mirroring::HORIZONTAL : Mirroring::VERTICAL;
            break;
        case 0x9003:
            irqEnable = (data & 0x80) != 0;
            irqActive = false;
            break;
        case 0x9004:
            irqCounter = irqReload;
            irqActive = false;
            break;
        case 0x9005:
            irqReload = (uint16_t)((irqReload & 0x00FF) | (data << 8));
            break;
        case 0x9006:
            irqReload = (uint16_t)((irqReload & 0xFF00) | data);
            break;
        default:
            if (a >= 0xB000 && a <= 0xB007) {
                chr[a - 0xB000] = data;
            }
            break;
        }
    }
    bool ppuMapRead(uint16_t a, uint32_t &mapped) override {
        if (a > 0x1FFF)
            return false;
        uint8_t idx = (uint8_t)(a >> 10);
        mapped = (uint32_t)chr[idx] * 0x0400 + (a & 0x03FF);
        return true;
    }
    bool ppuMapWrite(uint16_t a, uint32_t &mapped) override {
        return mapper_helpers::chrRamWrite(a, mapped, chrBanks);
    }
    Mirroring mirror() const override { return mirrorMode; }
    bool hasDynamicMirror() const override { return true; }

    void clock() override {
        if (!irqEnable || irqCounter == 0)
            return;
        if (--irqCounter == 0)
            irqActive = true;
    }
    bool irqState() const override { return irqActive; }
    void irqClear() override { irqActive = false; }

private:
    std::array<uint8_t, 3> prg{};
    std::array<uint8_t, 8> chr{};
    Mirroring mirrorMode = Mirroring::VERTICAL;
    bool irqEnable = false, irqActive = false;
    uint16_t irqReload = 0, irqCounter = 0;
public:
    std::unique_ptr<Mapper> clone() const override { return std::make_unique<Mapper065>(*this); }
    const char* name() const override { return "Irem H3001. 16-bit licznik IRQ (CPU cycle) z reloadem."; }
};

REGISTER_MAPPER(65, Mapper065)
