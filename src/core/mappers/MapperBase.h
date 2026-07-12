#pragma once

#include <cstdint>
#include "../AudioSettings.h"
#include "../DelayedPin.h"

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
    Mapper(uint16_t prgBanks, uint8_t chrBanks)
        : prgBanks(prgBanks), chrBanks(chrBanks) {}
    virtual ~Mapper() = default;

    virtual bool cpuMapRead(uint16_t addr, uint32_t &mapped, uint8_t &data) = 0;
    virtual void cpuMapWrite(uint16_t addr, uint32_t &mapped, uint8_t data) = 0;
    virtual bool ppuMapRead(uint16_t addr, uint32_t &mapped) = 0;
    virtual bool ppuMapWrite(uint16_t addr, uint32_t &mapped) = 0;
    virtual bool cpuReadDirect(uint16_t, uint8_t &) { return false; }
    virtual bool cpuWriteDirect(uint16_t, uint8_t) { return false; }
    virtual bool ppuReadDirect(uint16_t, uint8_t &) { return false; }
    virtual bool ppuWriteDirect(uint16_t, uint8_t) { return false; }

    virtual void reset() {}

    virtual Mirroring mirror() const { return Mirroring::HORIZONTAL; }
    virtual bool hasDynamicMirror() const { return false; }

    virtual bool irqState() const { return false; }
    virtual void irqClear() {}

    virtual void ppuAddress(uint16_t /*addr*/) {}
    virtual void ppuReadCycle(uint16_t /*addr*/) {}
    virtual void clockPpu() { a12LowQualified.tick(); }

    virtual void clock() {}

    virtual float audioOutput() const { return 0.0f; }

    virtual bool hasBusConflicts() const { return false; }
    virtual bool hasPrgRam() const { return true; }

    virtual void setAudioSettings(AudioSettings& settings) {}

    virtual const char* name() const = 0;

protected:
    bool a12RisingEdge(uint16_t addr) {
        const bool a12High = (addr & 0x1000) != 0;
        if (a12High == a12) return false;
        a12 = a12High;
        if (!a12) {
            a12LowQualified.set(true, 12);
            return false;
        }
        const bool edge = a12LowQualified.get();
        a12LowQualified.force(false);
        return edge;
    }

    void resetA12() {
        a12 = false;
        a12LowQualified.force(false);
        a12LowQualified.set(true, 12);
    }

    uint16_t prgBanks;
    uint8_t chrBanks;

private:
    bool a12 = false;
    DelayedPin<bool> a12LowQualified{false};
};