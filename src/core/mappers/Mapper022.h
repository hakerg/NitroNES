#pragma once
#include "MapperBase.h"
#include "MapperRegistry.h"
#include <array>

class Mapper022 : public Mapper {
public:
    Mapper022(uint8_t prg, uint8_t chr) : Mapper(prg, chr) { reset(); }

    void reset() override {
        prgReg = {};
        chrReg = {};
        mirrorMode = Mirroring::VERTICAL;
    }

    bool cpuMapRead(uint16_t addr, uint32_t& mapped, uint8_t&) override {
        if (addr < 0x8000) return false;
        const uint8_t total = prgBanks * 2;
        uint8_t bank;
        if (addr < 0xA000) bank = prgReg[0];
        else if (addr < 0xC000) bank = prgReg[1];
        else if (addr < 0xE000) bank = total - 2;
        else bank = total - 1;
        mapped = (uint32_t)mapper_helpers::maskBank(bank, total) * 0x2000
            + (addr & 0x1FFF);
        return true;
    }

    void cpuMapWrite(uint16_t addr, uint32_t&, uint8_t data) override {
        if (addr < 0x8000) return;
        const uint16_t reg = remapAddress(addr);
        switch (reg & 0xF000) {
        case 0x8000:
            prgReg[0] = data & 0x0F;
            break;
        case 0x9000:
            switch (data & 0x03) {
            case 0: mirrorMode = Mirroring::VERTICAL; break;
            case 1: mirrorMode = Mirroring::HORIZONTAL; break;
            case 2: mirrorMode = Mirroring::ONESCREEN_LO; break;
            case 3: mirrorMode = Mirroring::ONESCREEN_HI; break;
            }
            break;
        case 0xA000:
            prgReg[1] = data & 0x0F;
            break;
        case 0xB000:
        case 0xC000:
        case 0xD000:
        case 0xE000: {
            const uint8_t slot = ((reg >> 12) - 0x0B) * 2
                + ((reg >> 1) & 0x01);
            if (reg & 0x01)
                chrReg[slot] = (chrReg[slot] & 0x0F) | ((data & 0x0F) << 4);
            else
                chrReg[slot] = (chrReg[slot] & 0xF0) | (data & 0x0F);
            break;
        }
        default:
            break;
        }
    }

    bool ppuMapRead(uint16_t addr, uint32_t& mapped) override {
        if (addr > 0x1FFF) return false;
        const uint32_t total = chrBanks ? (uint32_t)chrBanks * 8 : 8;
        const uint8_t slot = addr >> 10;
        const uint32_t bank = (chrReg[slot] >> 1) % total;
        mapped = bank * 0x0400 + (addr & 0x03FF);
        return true;
    }

    bool ppuMapWrite(uint16_t addr, uint32_t& mapped) override {
        if (chrBanks != 0 || addr > 0x1FFF) return false;
        const uint8_t slot = addr >> 10;
        mapped = ((chrReg[slot] >> 1) & 0x07) * 0x0400 + (addr & 0x03FF);
        return true;
    }

    Mirroring mirror() const override { return mirrorMode; }
    bool hasDynamicMirror() const override { return true; }
    bool hasPrgRam() const override { return false; }

private:
    static uint16_t remapAddress(uint16_t addr) {
        return (addr & 0xFFFC) | ((addr & 0x01) << 1) | ((addr & 0x02) >> 1);
    }

    std::array<uint8_t, 2> prgReg{};
    std::array<uint8_t, 8> chrReg{};
    Mirroring mirrorMode = Mirroring::VERTICAL;
    const char* name() const override { return "VRC2"; }
};

REGISTER_MAPPER(22, Mapper022)
