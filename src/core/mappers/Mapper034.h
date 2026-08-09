#pragma once
#include "MapperBase.h"
#include "MapperRegistry.h"

class Mapper034 : public Mapper {
public:
    using Mapper::Mapper;

    void reset() override {
        prg = (prgBanks / 2) > 0 ? prgBanks / 2 - 1 : 0;
        chr0 = 0;
        chr1 = 1;
    }

    bool hasBusConflicts() const override { return true; }

    bool cpuMapRead(uint16_t addr, uint32_t &mapped, uint8_t &) override {
        if (addr < 0x8000)
            return false;
        mapped = mapper_helpers::mapPrg32k(addr, prg, prgBanks);
        return true;
    }

    void cpuMapWrite(uint16_t addr, uint32_t &, uint8_t data) override {
        if (addr == 0x7FFD)
            prg = data;
        else if (addr == 0x7FFE)
            chr0 = data;
        else if (addr == 0x7FFF)
            chr1 = data;
        else if (addr >= 0x8000)
            prg = data;
    }

    bool ppuMapRead(uint16_t addr, uint32_t &mapped) override {
        if (addr > 0x1FFF)
            return false;
        if (chrBanks == 0) {
            mapped = addr;
            return true;
        }
        uint8_t bank = addr < 0x1000 ? chr0 : chr1;
        mapped = (uint32_t)mapper_helpers::maskBank(bank, chrBanks * 2) * 0x1000 + (addr & 0x0FFF);
        return true;
    }

    bool ppuMapWrite(uint16_t addr, uint32_t &mapped) override {
        return mapper_helpers::chrRamWrite(addr, mapped, chrBanks);
    }

private:
    uint8_t prg = 0;
    uint8_t chr0 = 0;
    uint8_t chr1 = 1;
public:
    std::unique_ptr<Mapper> clone() const override { return std::make_unique<Mapper034>(*this); }
    const char* name() const override { return "BxROM"; }
};

REGISTER_MAPPER(34, Mapper034)
