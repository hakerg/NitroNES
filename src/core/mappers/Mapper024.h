#pragma once

#include "../audio_extensions/VRC6Audio.h"
#include "MapperBase.h"
#include "MapperRegistry.h"
#include <array>

// ============================================================================
// VRC6Mapper - wspolna baza dla Mapper024 (VRC6a) i Mapper026 (VRC6b).
// ----------------------------------------------------------------------------
// Realizuje banking PRG/CHR, mirroring i VRC IRQ zgodnie z nes_specs/vrc6.txt.
// Audio jest delegowane do samodzielnego modulu VRC6Audio (audio/VRC6Audio.h),
// dzieki czemu NSFPlayer moze uzywac wyjscia VRC6 bez tworzenia mappera.
//
// Roznice 024 vs 026: zamienione linie A0<->A1 dla rejestrow. Wewnatrz
// trzymamy "kanoniczne" numery jak dla VRC6a; cpuMapWrite() przemapowuje
// adresy dla VRC6b.
//
// Banki:
//   $6000-$7FFF: 8 KB PRG-RAM
//   $8000-$BFFF: 16 KB switchable
//   $C000-$DFFF: 8 KB switchable
//   $E000-$FFFF: 8 KB fixed = last
//   PPU $0000-$1FFF: 8x 1 KB switchable (rejestry R0..R7 z $D000/$E000)
//
// IRQ (VRC IRQ): tryb scanline lub CPU cycle, latch reload, A=ack flag.
// ============================================================================
class VRC6Mapper : public Mapper {
public:
    VRC6Mapper(uint8_t prg, uint8_t chr) : Mapper(prg, chr) { reset(); }

    void reset() override {
        prg16 = 0;
        prg8 = 0;
        for (auto &r : chrReg)
            r = 0;
        bMode = 0;
        ppuExt = false;
        ppuA10rule = true;
        prgRamEnable = false;
        irqLatch = 0;
        irqMode = false;
        irqEnable = false;
        irqEnableOnAck = false;
        irqCounter = 0;
        irqPrescaler = 341;
        irqActive = false;
        mirrorMode = Mirroring::HORIZONTAL;
        audio.reset();
    }

    bool cpuMapRead(uint16_t addr, uint32_t &mapped, uint8_t &) override {
        if (addr < 0x8000)
            return false;
        uint8_t total8k = (uint8_t)(prgBanks * 2);
        uint8_t bank;
        if (addr < 0xC000) {
            uint8_t base = (uint8_t)(prg16 * 2);
            bank = (uint8_t)(base | ((addr >> 13) & 0x01));
        } else if (addr < 0xE000) {
            bank = prg8;
        } else {
            bank = (uint8_t)(total8k - 1);
        }
        mapped = (uint32_t)mapper_helpers::maskBank(bank, total8k) * 0x2000 +
                 (addr & 0x1FFF);
        return true;
    }

    void cpuMapWrite(uint16_t addr, uint32_t &, uint8_t data) override {
        if (addr < 0x8000)
            return;
        uint16_t canon = addr & 0xF003;
        canon = remapLow(canon); // zamiana A0<->A1 dla VRC6b
        writeReg(canon, data);
    }

    bool ppuMapRead(uint16_t addr, uint32_t &mapped) override {
        if (addr > 0x1FFF)
            return false;
        uint8_t slot = (uint8_t)(addr >> 10);
        uint8_t bank;
        switch (bMode & 0x03) {
        default:
        case 0:
            bank = chrReg[slot];
            break;
        case 1: {
            static const uint8_t map[8] = {0, 0, 1, 1, 2, 2, 3, 3};
            uint8_t r = chrReg[map[slot]];
            if (ppuA10rule)
                bank = (uint8_t)((r & 0xFE) | ((addr >> 10) & 0x01));
            else
                bank = r;
            break;
        }
        case 2:
        case 3: {
            if (slot < 4)
                bank = chrReg[slot];
            else {
                static const uint8_t map[4] = {4, 4, 5, 5};
                uint8_t r = chrReg[map[slot - 4]];
                if (ppuA10rule)
                    bank = (uint8_t)((r & 0xFE) | ((addr >> 10) & 0x01));
                else
                    bank = r;
            }
            break;
        }
        }
        uint32_t total1k = (uint32_t)chrBanks * 8;
        uint32_t b1k = bank;
        if (total1k)
            b1k %= total1k;
        mapped = b1k * 0x0400 + (addr & 0x03FF);
        return true;
    }
    bool ppuMapWrite(uint16_t addr, uint32_t &mapped) override {
        return mapper_helpers::chrRamWrite(addr, mapped, chrBanks);
    }

    Mirroring mirror() const override { return mirrorMode; }
    bool hasDynamicMirror() const override { return true; }

    // VRC IRQ + 1 tick audio na cykl CPU.
    void clock() override {
        audio.clock();
        if (!irqEnable)
            return;
        if (!irqMode) {
            irqPrescaler -= 3;
            while (irqPrescaler <= 0) {
                irqPrescaler += 341;
                tickIrqCounter();
            }
        } else {
            tickIrqCounter();
        }
    }
    bool irqState() const override { return irqActive; }
    void irqClear() override { irqActive = false; }

    float audioOutput() const override { return audio.output(); }

    void setAudioSettings(AudioSettings& settings) override {
        audio.setSettings(settings);
    }

protected:
    virtual bool swapA01() const = 0;

private:
    uint16_t remapLow(uint16_t canon) const {
        if (!swapA01())
            return canon;
        uint16_t lo = canon & 0x0003;
        uint16_t swapped = (uint16_t)(((lo & 0x01) << 1) | ((lo & 0x02) >> 1));
        return (uint16_t)((canon & ~0x0003) | swapped);
    }

    void writeReg(uint16_t reg, uint8_t data) {
        switch (reg & 0xF003) {
        case 0x8000:
        case 0x8001:
        case 0x8002:
        case 0x8003:
            prg16 = data & 0x0F;
            break;

        case 0x9000:
        case 0x9001:
        case 0x9002:
        case 0x9003:
        case 0xA000:
        case 0xA001:
        case 0xA002:
        case 0xB000:
        case 0xB001:
        case 0xB002:
            audio.writeReg(reg, data);
            break;
        case 0xA003: /* niewykorzystane w VRC6 audio */
            break;

        case 0xB003:
            bMode = data & 0x03;
            ppuExt = (data & 0x10) != 0;
            ppuA10rule = (data & 0x20) != 0;
            prgRamEnable = (data & 0x80) != 0;
            switch (data & 0x0C) {
            case 0x00:
                mirrorMode = Mirroring::VERTICAL;
                break;
            case 0x04:
                mirrorMode = Mirroring::HORIZONTAL;
                break;
            case 0x08:
                mirrorMode = Mirroring::ONESCREEN_LO;
                break;
            case 0x0C:
                mirrorMode = Mirroring::ONESCREEN_HI;
                break;
            }
            break;

        case 0xC000:
        case 0xC001:
        case 0xC002:
        case 0xC003:
            prg8 = data & 0x1F;
            break;

        case 0xD000:
            chrReg[0] = data;
            break;
        case 0xD001:
            chrReg[1] = data;
            break;
        case 0xD002:
            chrReg[2] = data;
            break;
        case 0xD003:
            chrReg[3] = data;
            break;
        case 0xE000:
            chrReg[4] = data;
            break;
        case 0xE001:
            chrReg[5] = data;
            break;
        case 0xE002:
            chrReg[6] = data;
            break;
        case 0xE003:
            chrReg[7] = data;
            break;

        case 0xF000:
            irqLatch = data;
            break;
        case 0xF001:
            irqMode = (data & 0x04) != 0;
            irqEnable = (data & 0x02) != 0;
            irqEnableOnAck = (data & 0x01) != 0;
            if (irqEnable) {
                irqCounter = irqLatch;
                irqPrescaler = 341;
            }
            irqActive = false;
            break;
        case 0xF002:
            irqActive = false;
            irqEnable = irqEnableOnAck;
            break;
        default:
            break;
        }
    }

    void tickIrqCounter() {
        if (irqCounter == 0xFF) {
            irqCounter = irqLatch;
            irqActive = true;
        } else {
            irqCounter++;
        }
    }

    // PRG/CHR/banking
    uint8_t prg16 = 0, prg8 = 0;
    std::array<uint8_t, 8> chrReg{};
    uint8_t bMode = 0;
    bool ppuExt = false, ppuA10rule = true, prgRamEnable = false;
    Mirroring mirrorMode = Mirroring::HORIZONTAL;

    // VRC IRQ
    uint8_t irqLatch = 0;
    int16_t irqPrescaler = 341;
    uint8_t irqCounter = 0;
    bool irqMode = false, irqEnable = false, irqEnableOnAck = false,
         irqActive = false;

    // Audio (delegowane do samodzielnego modulu).
    VRC6Audio audio;
};

// Konami VRC6a - Akumajou Densetsu (Castlevania III JP).
class Mapper024 : public VRC6Mapper {
public:
    using VRC6Mapper::VRC6Mapper;

protected:
    bool swapA01() const override { return false; }
};

REGISTER_MAPPER(24, Mapper024)
