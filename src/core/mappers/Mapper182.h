#pragma once
#include "Mapper004.h"
#include "MapperRegistry.h"

// ----------------------------------------------------------------------------
// Mapper 182 - MMC3 z poprzestawianymi rejestrami
// ----------------------------------------------------------------------------
// Translacja zapisow (zrodlo: nes_specs/mappers/182.txt):
//   M182        MMC3
//   $8001  ->   $A000  (mirroring)
//   $A000  ->   $8000  (bank select, dodatkowo scramble R:x)
//   $C000  ->   $8001  (bank data)
//   $C001  ->   $C000+$C001 (reload + clear counter)
//   $E000  ->   $E000  (irq disable)
//   $E001  ->   $E001  (irq enable)
// Mapowanie R:0..7 (M182 -> MMC3): 0,3,1,5,6,7,2,4
// ----------------------------------------------------------------------------
class Mapper182 : public Mapper004 {
public:
    using Mapper004::Mapper004;

    void cpuMapWrite(uint16_t addr, uint32_t &mapped, uint8_t data) override {
        if (addr < 0x8000) {
            Mapper004::cpuMapWrite(addr, mapped, data);
            return;
        }
        uint16_t reg = addr & 0xE001;
        switch (reg) {
        case 0x8001:
            Mapper004::cpuMapWrite(0xA000, mapped, data);
            return; // mirroring
        case 0xA000: {
            // bank select - scramble pola "target reg" R0..7
            static const uint8_t map[8] = {0, 3, 1, 5, 6, 7, 2, 4};
            uint8_t newTarget = map[data & 0x07];
            uint8_t translated = (uint8_t)((data & 0xC0) | newTarget);
            Mapper004::cpuMapWrite(0x8000, mapped, translated);
            return;
        }
        case 0xC000:
            Mapper004::cpuMapWrite(0x8001, mapped, data);
            return; // bank data
        case 0xC001:
            Mapper004::cpuMapWrite(0xC000, mapped, data);
            Mapper004::cpuMapWrite(0xC001, mapped, data);
            return;
        case 0xE000:
            Mapper004::cpuMapWrite(0xE000, mapped, data);
            return;
        case 0xE001:
            Mapper004::cpuMapWrite(0xE001, mapped, data);
            return;
        default:
            return;
        }
    }
    std::unique_ptr<Mapper> clone() const override { return std::make_unique<Mapper182>(*this); }
    const char* name() const override { return "MMC3 z poprzestawianymi rejestrami"; }
};

REGISTER_MAPPER(182, Mapper182)
