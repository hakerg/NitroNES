#pragma once
#include "MapperBase.h"
#include "MapperRegistry.h"

// Mapper 185 - protect. CHR enable na podstawie warto�ci w rejestrze; bus
// conflicts.
class Mapper185 : public Mapper {
public:
    using Mapper::Mapper;
    void reset() override { reg = 0; }
    bool hasBusConflicts() const override { return true; }
    bool cpuMapRead(uint16_t a, uint32_t &mapped, uint8_t &) override {
        if (a < 0x8000)
            return false;
        mapped = a & 0x7FFF;
        return true;
    }
    void cpuMapWrite(uint16_t a, uint32_t &, uint8_t data) override {
        if (a < 0x8000)
            return;
        reg = data;
    }
    bool ppuMapRead(uint16_t a, uint32_t &mapped) override {
        if (a > 0x1FFF)
            return false;
        // CHR w��czone gdy (reg & 0x0F) != 0 i reg != 0x13
        bool enabled = ((reg & 0x0F) != 0) && (reg != 0x13);
        if (!enabled) {
            // open bus � zwracamy LSB adresu jako przybli�enie
            mapped = 0xFFFFFFFF;
            return false;
        }
        mapped = a;
        return true;
    }
    bool ppuMapWrite(uint16_t a, uint32_t &mapped) override {
        return mapper_helpers::chrRamWrite(a, mapped, chrBanks);
    }

private:
    uint8_t reg = 0;
public:
    std::unique_ptr<Mapper> clone() const override { return std::make_unique<Mapper185>(*this); }
    const char* name() const override { return "protect. CHR enable na podstawie warto�ci w rejestrze; bus"; }
};

REGISTER_MAPPER(185, Mapper185)
