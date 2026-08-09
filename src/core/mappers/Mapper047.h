#pragma once
#include "Mapper004.h"

// ----------------------------------------------------------------------------
// Mapper 047 - Super Spike V'Ball + Nintendo World Cup (MMC3 multicart)
//   $6000-7FFF: [.... ...B] block select (2 bloki po 128K PRG/CHR)
// ----------------------------------------------------------------------------
class Mapper047 : public Mapper004 {
public:
    using Mapper004::Mapper004;

    void reset() override {
        Mapper004::reset();
        block = 0;
    }

    void cpuMapWrite(uint16_t addr, uint32_t &mapped, uint8_t data) override {
        if (addr >= 0x6000 && addr < 0x8000) {
            block = data & 0x01;
            return;
        }
        Mapper004::cpuMapWrite(addr, mapped, data);
    }

    bool cpuMapRead(uint16_t addr, uint32_t &mapped, uint8_t &data) override {
        if (!Mapper004::cpuMapRead(addr, mapped, data))
            return false;
        mapped = (mapped & 0x1FFFF) | ((uint32_t)block * 128u * 1024u);
        return true;
    }

    bool ppuMapRead(uint16_t addr, uint32_t &mapped) override {
        if (!Mapper004::ppuMapRead(addr, mapped))
            return false;
        mapped = (mapped & 0x1FFFF) | ((uint32_t)block * 128u * 1024u);
        return true;
    }

private:
    uint8_t block = 0;
public:
    std::unique_ptr<Mapper> clone() const override { return std::make_unique<Mapper047>(*this); }
    const char* name() const override { return "Super Spike V'Ball + Nintendo World Cup (MMC3 multicart)"; }
};

REGISTER_MAPPER(47, Mapper047)
