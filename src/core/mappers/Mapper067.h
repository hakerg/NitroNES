#pragma once
#include "MapperBase.h"
#include "MapperRegistry.h"
#include <array>

// ----------------------------------------------------------------------------
// Mapper 067 - Sunsoft-3 (Fantasy Zone 2 J)
// ----------------------------------------------------------------------------
// Range,Mask: $8000-FFFF, $F800
//   $8800: CHR Reg 0 (2K @ $0000)
//   $9800: CHR Reg 1 (2K @ $0800)
//   $A800: CHR Reg 2 (2K @ $1000)
//   $B800: CHR Reg 3 (2K @ $1800)
//   $C800: IRQ Load (write-twice: high first, then low). Bezposrednio ustawia
//   licznik. $D800: [...E ....] IRQ Enable (1=enable); jakikolwiek zapis kasuje
//   IRQ i toggle. $E800: [.... ..MM] mirroring (00=V,01=H,10=1ScA,11=1ScB)
//   $F800: PRG Reg (16K @ $8000); {-1} @ $C000.
// IRQ: licznik 16-bit dekrementowany kazdym cyklem CPU. Wrap $0000->$FFFF
// generuje IRQ i wylacza siebie.
// ----------------------------------------------------------------------------
class Mapper067 : public Mapper {
public:
    using Mapper::Mapper;

    void reset() override {
        chr.fill(0);
        prg = 0;
        mirrorMode = Mirroring::VERTICAL;
        irqCounter = 0;
        irqEnable = false;
        irqActive = false;
        irqHighNext = true;
    }

    bool cpuMapRead(uint16_t addr, uint32_t &mapped, uint8_t &) override {
        if (addr < 0x8000)
            return false;
        mapped = mapper_helpers::mapPrg16k_fixedHi(addr, prg, prgBanks);
        return true;
    }
    void cpuMapWrite(uint16_t addr, uint32_t &, uint8_t data) override {
        if (addr < 0x8000)
            return;
        uint16_t r = addr & 0xF800;
        switch (r) {
        case 0x8800:
            chr[0] = data;
            break;
        case 0x9800:
            chr[1] = data;
            break;
        case 0xA800:
            chr[2] = data;
            break;
        case 0xB800:
            chr[3] = data;
            break;
        case 0xC800:
            if (irqHighNext)
                irqCounter =
                    (uint16_t)((irqCounter & 0x00FF) | ((uint16_t)data << 8));
            else
                irqCounter = (uint16_t)((irqCounter & 0xFF00) | (uint16_t)data);
            irqHighNext = !irqHighNext;
            break;
        case 0xD800:
            irqEnable = (data & 0x10) != 0;
            irqActive = false;
            irqHighNext = true;
            break;
        case 0xE800:
            switch (data & 0x03) {
            case 0:
                mirrorMode = Mirroring::VERTICAL;
                break;
            case 1:
                mirrorMode = Mirroring::HORIZONTAL;
                break;
            case 2:
                mirrorMode = Mirroring::ONESCREEN_LO;
                break;
            case 3:
                mirrorMode = Mirroring::ONESCREEN_HI;
                break;
            }
            break;
        case 0xF800:
            prg = data;
            break;
        }
    }
    bool ppuMapRead(uint16_t addr, uint32_t &mapped) override {
        if (addr > 0x1FFF)
            return false;
        uint8_t slot = (uint8_t)((addr >> 11) & 0x03); // 2K slots
        mapped = (uint32_t)chr[slot] * 0x0800 + (addr & 0x07FF);
        return true;
    }
    bool ppuMapWrite(uint16_t addr, uint32_t &mapped) override {
        return mapper_helpers::chrRamWrite(addr, mapped, chrBanks);
    }

    Mirroring mirror() const override { return mirrorMode; }
    bool hasDynamicMirror() const override { return true; }

    void clock() override {
        if (!irqEnable)
            return;
        if (irqCounter == 0) {
            irqActive = true;
            irqEnable = false;
            irqCounter = 0xFFFF;
        } else {
            irqCounter--;
        }
    }
    bool irqState() const override { return irqActive; }
    void irqClear() override { irqActive = false; }

private:
    std::array<uint8_t, 4> chr{};
    uint8_t prg = 0;
    Mirroring mirrorMode = Mirroring::VERTICAL;
    uint16_t irqCounter = 0;
    bool irqEnable = false, irqActive = false, irqHighNext = true;
public:
    std::unique_ptr<Mapper> clone() const override { return std::make_unique<Mapper067>(*this); }
    const char* name() const override { return "Sunsoft-3 (Fantasy Zone 2 J)"; }
};

REGISTER_MAPPER(67, Mapper067)
