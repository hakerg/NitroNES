#pragma once
#include "MapperBase.h"
#include "MapperRegistry.h"

// Mapper 013 - CPROM. PRG fixed 32KB, CHR-RAM 16KB; bank 4KB @ $1000.
// Ma bus conflicts.
class Mapper013 : public Mapper {
public:
    using Mapper::Mapper;
    void reset() override { chrBank = 0; }
    bool hasBusConflicts() const override { return true; }

    bool cpuMapRead(uint16_t addr, uint32_t &mapped, uint8_t &) override {
        if (addr < 0x8000)
            return false;
        mapped = addr & 0x7FFF;
        return true;
    }
    void cpuMapWrite(uint16_t addr, uint32_t &, uint8_t data) override {
        if (addr < 0x8000)
            return;
        chrBank = data & 0x03;
    }
    bool ppuMapRead(uint16_t addr, uint32_t &mapped) override {
        if (addr > 0x1FFF)
            return false;
        if (addr < 0x1000)
            mapped = addr;
        else
            mapped = (uint32_t)chrBank * 0x1000 + (addr & 0x0FFF);
        return true;
    }
    bool ppuMapWrite(uint16_t addr, uint32_t &mapped) override {
        if (addr > 0x1FFF)
            return false;
        if (addr < 0x1000)
            mapped = addr;
        else
            mapped = (uint32_t)chrBank * 0x1000 + (addr & 0x0FFF);
        return true;
    }

private:
    uint8_t chrBank = 0;
public:
    std::unique_ptr<Mapper> clone() const override { return std::make_unique<Mapper013>(*this); }
    const char* name() const override { return "CPROM. PRG fixed 32KB, CHR-RAM 16KB; bank 4KB @ $1000."; }
};

REGISTER_MAPPER(13, Mapper013)
