#pragma once

#include <cstdint>
#include "../AudioSettings.h"

enum class Mirroring {
    HORIZONTAL,
    VERTICAL,
    ONESCREEN_LO,
    ONESCREEN_HI,
    FOURSCREEN,
};

namespace mapper_helpers {

inline uint8_t maskBank(uint8_t bank, uint8_t numBanks) {
    if (numBanks == 0)
        return 0;
    if ((numBanks & (numBanks - 1)) == 0)
        return bank & (numBanks - 1);
    return bank % numBanks;
}

inline uint32_t mapPrg16k_fixedHi(uint16_t addr, uint8_t bankLo,
                                  uint8_t prgBanks) {
    if (addr < 0xC000)
        return (uint32_t)maskBank(bankLo, prgBanks) * 0x4000 + (addr & 0x3FFF);
    return (uint32_t)maskBank(prgBanks - 1, prgBanks) * 0x4000 +
           (addr & 0x3FFF);
}

inline uint32_t mapPrg16k_fixedLo(uint16_t addr, uint8_t bankHi,
                                  uint8_t prgBanks) {
    if (addr < 0xC000)
        return (uint32_t)0 * 0x4000 + (addr & 0x3FFF);
    return (uint32_t)maskBank(bankHi, prgBanks) * 0x4000 + (addr & 0x3FFF);
}

inline uint32_t mapPrg32k(uint16_t addr, uint8_t bank32, uint8_t prgBanks) {
    uint8_t n = prgBanks / 2;
    if (n == 0)
        n = 1;
    return (uint32_t)maskBank(bank32, n) * 0x8000 + (addr & 0x7FFF);
}

inline uint32_t mapChr8k(uint16_t addr, uint8_t bank, uint8_t chrBanks) {
    if (chrBanks == 0)
        return addr & 0x1FFF;
    return (uint32_t)maskBank(bank, chrBanks) * 0x2000 + (addr & 0x1FFF);
}

inline bool chrRamWrite(uint16_t addr, uint32_t &mapped, uint8_t chrBanks) {
    if (addr <= 0x1FFF && chrBanks == 0) {
        mapped = addr;
        return true;
    }
    return false;
}

} // namespace mapper_helpers

class Mapper {
public:
    Mapper(uint8_t prgBanks, uint8_t chrBanks)
        : prgBanks(prgBanks), chrBanks(chrBanks) {}
    virtual ~Mapper() = default;

    virtual bool cpuMapRead(uint16_t addr, uint32_t &mapped, uint8_t &data) = 0;
    virtual void cpuMapWrite(uint16_t addr, uint32_t &mapped, uint8_t data) = 0;
    virtual bool ppuMapRead(uint16_t addr, uint32_t &mapped) = 0;
    virtual bool ppuMapWrite(uint16_t addr, uint32_t &mapped) = 0;

    virtual void reset() {}

    virtual Mirroring mirror() const { return Mirroring::HORIZONTAL; }
    virtual bool hasDynamicMirror() const { return false; }

    virtual bool irqState() const { return false; }
    virtual void irqClear() {}

    virtual void clockA12(uint16_t /*addr*/, uint64_t /*ppuCycle*/) {}

    virtual void clock() {}

    virtual float audioOutput() const { return 0.0f; }

    virtual bool hasBusConflicts() const { return false; }

    virtual void setAudioSettings(AudioSettings& settings) {}

protected:
    bool a12RisingEdge(uint16_t addr, uint64_t ppuCycle) {
        const bool a12High = (addr & 0x1000) != 0;
        bool edge = false;
        if (a12High) {
            if (!a12Prev && (ppuCycle - a12FallCycle) >= 12) edge = true;
        } else if (a12Prev) {
            a12FallCycle = ppuCycle;
        }
        a12Prev = a12High;
        return edge;
    }

    uint8_t prgBanks;
    uint8_t chrBanks;

private:
    bool a12Prev = false;
    uint64_t a12FallCycle = 0;
};