#pragma once
#include "MapperBase.h"
#include "MapperRegistry.h"
#include <array>

// Mapper 073 - VRC3. 16-bit licznik IRQ nap�dzany cyklem CPU.
class Mapper073 : public Mapper {
public:
    using Mapper::Mapper;
    void reset() override {
        prg = 0;
        reload = 0;
        counter = 0;
        irqMode = false;
        irqEnable = false;
        irqEnableOnAck = false;
        irqActive = false;
    }
    bool cpuMapRead(uint16_t a, uint32_t &mapped, uint8_t &) override {
        if (a < 0x8000)
            return false;
        if (a < 0xC000)
            mapped = (uint32_t)prg * 0x4000 + (a & 0x3FFF);
        else
            mapped = (uint32_t)(prgBanks - 1) * 0x4000 + (a & 0x3FFF);
        return true;
    }
    void cpuMapWrite(uint16_t a, uint32_t &, uint8_t data) override {
        if (a < 0x8000)
            return;
        switch (a & 0xF000) {
        case 0x8000:
            reload = (uint16_t)((reload & 0xFFF0) | (data & 0x0F));
            break;
        case 0x9000:
            reload = (uint16_t)((reload & 0xFF0F) | ((data & 0x0F) << 4));
            break;
        case 0xA000:
            reload = (uint16_t)((reload & 0xF0FF) | ((data & 0x0F) << 8));
            break;
        case 0xB000:
            reload = (uint16_t)((reload & 0x0FFF) | ((data & 0x0F) << 12));
            break;
        case 0xC000:
            irqMode = (data & 0x04) != 0;
            irqEnable = (data & 0x02) != 0;
            irqEnableOnAck = (data & 0x01) != 0;
            irqActive = false;
            if (irqEnable)
                counter = reload;
            break;
        case 0xD000:
            irqActive = false;
            irqEnable = irqEnableOnAck;
            break;
        case 0xF000:
            prg = data & 0x0F;
            break;
        }
    }
    bool ppuMapRead(uint16_t a, uint32_t &mapped) override {
        if (a > 0x1FFF)
            return false;
        mapped = a;
        return true;
    }
    bool ppuMapWrite(uint16_t a, uint32_t &mapped) override {
        return mapper_helpers::chrRamWrite(a, mapped, chrBanks);
    }
    void clock() override {
        if (!irqEnable)
            return;
        if (irqMode) { // 8-bit
            uint8_t lo = (uint8_t)(counter & 0xFF);
            if (lo == 0xFF) {
                counter = (uint16_t)((counter & 0xFF00) | (reload & 0x00FF));
                irqActive = true;
            } else {
                counter = (uint16_t)((counter & 0xFF00) | ((lo + 1) & 0xFF));
            }
        } else {
            if (counter == 0xFFFF) {
                counter = reload;
                irqActive = true;
            } else {
                counter++;
            }
        }
    }
    bool irqState() const override { return irqActive; }
    void irqClear() override { irqActive = false; }

private:
    uint8_t prg = 0;
    uint16_t reload = 0, counter = 0;
    bool irqMode = false, irqEnable = false, irqEnableOnAck = false,
         irqActive = false;
};

REGISTER_MAPPER(73, Mapper073)
