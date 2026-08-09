#pragma once
#include "MapperBase.h"
#include "MapperRegistry.h"
#include <array>

class Mapper028 : public Mapper {
public:
    Mapper028(uint16_t prg, uint8_t chr) : Mapper(prg, chr) {
        mode = 0x0C;
        prgInner = 0x0F;
        prgOuter = 0xFF;
        updateMirror();
    }

    void reset() override {}

    bool cpuMapRead(uint16_t addr, uint32_t& mapped, uint8_t&) override {
        if (addr < 0x8000) return false;
        const uint16_t total16k = prgBanks;
        const uint16_t slot = addr >= 0xC000;
        const uint8_t size = (mode >> 4) & 0x03;
        const uint16_t innerMask = (2u << size) - 1u;
        const uint16_t base = ((uint16_t)prgOuter << 1) & ~innerMask;
        const uint16_t inner = base | (prgInner & innerMask);
        const uint16_t fixed = ((uint16_t)prgOuter << 1) | slot;
        uint16_t bank;
        switch ((mode >> 2) & 0x03) {
        case 0:
        case 1:
            bank =
                (((uint16_t)prgOuter << 1) & ~innerMask)
                | ((prgInner & (innerMask >> 1)) << 1)
                | slot;
            break;
        case 2:
            bank = slot ? inner : fixed;
            break;
        default:
            bank = slot ? fixed : inner;
            break;
        }
        mapped = (uint32_t)(bank % total16k) * 0x4000
            + (addr & 0x3FFF);
        return true;
    }

    void cpuMapWrite(uint16_t addr, uint32_t&, uint8_t data) override {
        if (addr >= 0x5000 && addr <= 0x5FFF) {
            selected = data & 0x81;
            return;
        }
        if (addr < 0x8000) return;
        switch (selected) {
        case 0x00:
            chrBank = data & 0x03;
            updateOneScreenMirror(data);
            break;
        case 0x01:
            prgInner = data & 0x0F;
            updateOneScreenMirror(data);
            break;
        case 0x80:
            mode = data & 0x3F;
            updateMirror();
            break;
        case 0x81:
            prgOuter = data;
            break;
        }
    }

    bool ppuMapRead(uint16_t addr, uint32_t& mapped) override {
        if (addr > 0x1FFF) return false;
        if (chrBanks == 0) return false;
        const uint32_t total = chrBanks ? chrBanks : 1;
        mapped = (uint32_t)(chrBank % total) * 0x2000 + (addr & 0x1FFF);
        return true;
    }

    bool ppuMapWrite(uint16_t, uint32_t&) override {
        return false;
    }

    bool ppuReadDirect(uint16_t addr, uint8_t& data) override {
        if (chrBanks != 0 || addr > 0x1FFF) return false;
        data = chrRam[(uint32_t)chrBank * 0x2000 + addr];
        return true;
    }

    bool ppuWriteDirect(uint16_t addr, uint8_t data) override {
        if (chrBanks != 0 || addr > 0x1FFF) return false;
        chrRam[(uint32_t)chrBank * 0x2000 + addr] = data;
        return true;
    }

    Mirroring mirror() const override { return mirrorMode; }
    bool hasDynamicMirror() const override { return true; }
    bool hasPrgRam() const override { return false; }

private:
    void updateMirror() {
        switch (mode & 0x03) {
        case 0: mirrorMode = Mirroring::ONESCREEN_LO; break;
        case 1: mirrorMode = Mirroring::ONESCREEN_HI; break;
        case 2: mirrorMode = Mirroring::VERTICAL; break;
        case 3: mirrorMode = Mirroring::HORIZONTAL; break;
        }
    }

    void updateOneScreenMirror(uint8_t data) {
        if ((mode & 0x02) == 0) {
            mode = (mode & ~0x01) | ((data >> 4) & 0x01);
            updateMirror();
        }
    }

    uint8_t selected = 0;
    uint8_t chrBank = 0;
    uint8_t prgInner = 0;
    uint8_t mode = 0;
    uint8_t prgOuter = 0;
    Mirroring mirrorMode = Mirroring::ONESCREEN_LO;
    std::array<uint8_t, 32768> chrRam{};
public:
    std::unique_ptr<Mapper> clone() const override { return std::make_unique<Mapper028>(*this); }
    const char* name() const override { return "Action 53"; }
};

REGISTER_MAPPER(28, Mapper028)
