#pragma once
#include "MapperBase.h"
#include "MapperRegistry.h"
#include <array>

class Mapper069 : public Mapper {
public:
    Mapper069(uint16_t prg, uint8_t chr) : Mapper(prg, chr) { reset(); }

    void reset() override {
        command = 0;
        workRam = 0;
        chr.fill(0);
        prg.fill(0);
        mirrorMode = Mirroring::VERTICAL;
        irqCounter = 0;
        irqCounterEnabled = false;
        irqEnabled = false;
        irqActive = false;
    }

    bool cpuMapRead(uint16_t addr, uint32_t &mapped, uint8_t &) override {
        uint16_t total8k = prgBanks * 2;
        if (addr >= 0x6000 && addr < 0x8000) {
            if (workRam & 0x40)
                return false;
            mapped = (uint32_t)mapper_helpers::maskBank(workRam & 0x3F, total8k) * 0x2000 + (addr & 0x1FFF);
            return true;
        }
        if (addr < 0x8000)
            return false;
        uint16_t slot = (addr - 0x8000) >> 13;
        uint16_t bank = slot < 3 ? prg[slot] : total8k - 1;
        mapped = (uint32_t)mapper_helpers::maskBank(bank, total8k) * 0x2000 + (addr & 0x1FFF);
        return true;
    }

    void cpuMapWrite(uint16_t addr, uint32_t &, uint8_t data) override {
        switch (addr & 0xE000) {
        case 0x8000:
            command = data & 0x0F;
            break;
        case 0xA000:
            writeCommand(data);
            break;
        }
    }

    bool ppuMapRead(uint16_t addr, uint32_t &mapped) override {
        if (addr > 0x1FFF)
            return false;
        uint16_t total1k = chrBanks * 8;
        mapped = (uint32_t)mapper_helpers::maskBank(chr[addr >> 10], total1k) * 0x0400 + (addr & 0x03FF);
        return true;
    }

    bool ppuMapWrite(uint16_t, uint32_t &) override { return false; }

    bool hasPrgRam() const override {
        return (workRam & 0xC0) == 0xC0;
    }

    Mirroring mirror() const override { return mirrorMode; }
    bool hasDynamicMirror() const override { return true; }

    void clock() override {
        if (!irqCounterEnabled)
            return;
        irqCounter--;
        if (irqCounter == 0xFFFF && irqEnabled)
            irqActive = true;
    }

    bool irqState() const override { return irqActive; }
    void irqClear() override { irqActive = false; }

private:
    void writeCommand(uint8_t data) {
        if (command < 8) {
            chr[command] = data;
            return;
        }
        if (command < 12) {
            if (command == 8)
                workRam = data;
            else
                prg[command - 9] = data & 0x3F;
            return;
        }
        switch (command) {
        case 12:
            switch (data & 0x03) {
            case 0: mirrorMode = Mirroring::VERTICAL; break;
            case 1: mirrorMode = Mirroring::HORIZONTAL; break;
            case 2: mirrorMode = Mirroring::ONESCREEN_LO; break;
            case 3: mirrorMode = Mirroring::ONESCREEN_HI; break;
            }
            break;
        case 13:
            irqEnabled = data & 0x01;
            irqCounterEnabled = data & 0x80;
            irqActive = false;
            break;
        case 14:
            irqCounter = (irqCounter & 0xFF00) | data;
            break;
        case 15:
            irqCounter = (irqCounter & 0x00FF) | ((uint16_t)data << 8);
            break;
        }
    }

    uint8_t command = 0;
    uint8_t workRam = 0;
    std::array<uint8_t, 8> chr{};
    std::array<uint8_t, 3> prg{};
    Mirroring mirrorMode = Mirroring::VERTICAL;
    uint16_t irqCounter = 0;
    bool irqCounterEnabled = false;
    bool irqEnabled = false;
    bool irqActive = false;
public:
    std::unique_ptr<Mapper> clone() const override { return std::make_unique<Mapper069>(*this); }
    const char* name() const override { return "FME-7"; }
};

REGISTER_MAPPER(69, Mapper069)
