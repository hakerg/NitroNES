#pragma once
#include "../audio_extensions/MMC5Audio.h"
#include "MapperBase.h"
#include "MapperRegistry.h"
#include <array>

class Mapper005 : public Mapper {
public:
    Mapper005(uint8_t prg, uint8_t chr) : Mapper(prg, chr) { reset(); }

    void reset() override {
        prgMode = 3;
        chrMode = 3;
        protectA = protectB = 0;
        exMode = 0;
        nametableMap = 0;
        fillTile = fillAttribute = 0;
        prgReg = {0, 0, 0, 0, 0xFF};
        chrA.fill(0);
        chrB.fill(0);
        chrHigh = 0;
        lastChrSetB = false;
        splitControl = splitScroll = splitBank = 0;
        irqTarget = irqCounter = 0;
        irqEnabled = irqPending = inFrame = false;
        multiplierA = multiplierB = 0;
        lastPpuAddress = 0xFFFF;
        repeatedAddress = 0;
        scanlinePending = false;
        ppuAccessCount = 0;
        ppuReadThisCpuCycle = false;
        idleCpuCycles = 0;
        lastTileOffset = 0;
        splitActive = false;
        splitFineY = 0;
        audio.reset();
    }

    bool cpuReadDirect(uint16_t addr, uint8_t& data) override {
        if (addr == 0x5015) {
            data = audio.status();
            return true;
        }
        if (addr == 0x5204) {
            data = (irqPending ? 0x80 : 0) | (inFrame ? 0x40 : 0);
            irqPending = false;
            return true;
        }
        if (addr == 0x5205 || addr == 0x5206) {
            const uint16_t product = (uint16_t)multiplierA * multiplierB;
            data = addr == 0x5205 ? product : product >> 8;
            return true;
        }
        if (addr >= 0x5C00 && addr <= 0x5FFF) {
            if (exMode < 2) return false;
            data = exRam[addr & 0x03FF];
            return true;
        }
        uint8_t bank;
        if (ramBank(addr, bank)) {
            data = prgRam[(uint32_t)(bank & 7) * 0x2000 + (addr & 0x1FFF)];
            return true;
        }
        return false;
    }

    bool cpuWriteDirect(uint16_t addr, uint8_t data) override {
        if ((addr >= 0x5000 && addr <= 0x5015) ||
            (addr >= 0x5100 && addr <= 0x5206) ||
            (addr >= 0x5C00 && addr <= 0x5FFF)) {
            writeRegister(addr, data);
            return true;
        }
        uint8_t bank;
        if (ramBank(addr, bank)) {
            if (ramWritable())
                prgRam[(uint32_t)(bank & 7) * 0x2000 + (addr & 0x1FFF)] = data;
            return true;
        }
        return false;
    }

    bool cpuMapRead(uint16_t addr, uint32_t& mapped, uint8_t&) override {
        if (addr < 0x8000) return false;
        const uint8_t slot = (addr - 0x8000) >> 13;
        const uint8_t reg = slotRegister(slot);
        if (slot < 3 && !(reg & 0x80)) return false;
        const uint8_t total = prgBanks * 2;
        mapped = (uint32_t)mapper_helpers::maskBank(prgBank(slot), total) * 0x2000
            + (addr & 0x1FFF);
        return true;
    }

    void cpuMapWrite(uint16_t, uint32_t&, uint8_t) override {}

    bool ppuMapRead(uint16_t addr, uint32_t& mapped) override {
        if (addr > 0x1FFF) return false;
        const uint32_t total = chrBanks ? (uint32_t)chrBanks * 8 : 8;
        const uint32_t offset = splitActive && useBackgroundSet()
            ? ((addr & 0x03F8) | splitFineY) : (addr & 0x03FF);
        mapped = (chrBank(addr) % total) * 0x0400 + offset;
        return true;
    }

    bool ppuMapWrite(uint16_t addr, uint32_t& mapped) override {
        if (chrBanks || addr > 0x1FFF) return false;
        mapped = (chrBank(addr) & 7) * 0x0400 + (addr & 0x03FF);
        return true;
    }

    bool ppuReadDirect(uint16_t addr, uint8_t& data) override {
        if (addr < 0x2000 || addr >= 0x3F00) return false;
        const uint16_t offset = addr & 0x0FFF;
        const uint8_t quadrant = offset >> 10;
        const uint8_t source = (nametableMap >> (quadrant * 2)) & 3;
        const uint16_t index = offset & 0x03FF;
        if (index < 0x03C0) {
            const uint8_t tile = ppuAccessCount ? (ppuAccessCount - 1) / 4 : 0;
            splitActive = isSplitTile(tile);
            if (splitActive) {
                const uint16_t y = (irqCounter + splitScroll) % 240;
                splitFineY = y & 7;
                lastTileOffset = (y >> 3) * 32 + (tile & 31);
                data = exRam[lastTileOffset];
                return true;
            }
        } else if (splitActive) {
            const uint8_t tile = ppuAccessCount ? (ppuAccessCount - 1) / 4 : 0;
            const uint16_t y = (irqCounter + splitScroll) % 240;
            data = exRam[0x03C0 + ((y >> 5) * 8) + ((tile & 31) >> 2)];
            return true;
        }
        if (exMode == 1 && index >= 0x03C0) {
            const uint8_t attr = exRam[lastTileOffset & 0x03FF] >> 6;
            data = attr * 0x55;
            return true;
        }
        if (index < 0x03C0) lastTileOffset = index;
        switch (source) {
        case 0: data = nametable[0][index]; break;
        case 1: data = nametable[1][index]; break;
        case 2: data = exMode <= 1 ? exRam[index] : 0; break;
        default: data = index < 0x03C0 ? fillTile : (fillAttribute & 3) * 0x55; break;
        }
        return true;
    }

    bool ppuWriteDirect(uint16_t addr, uint8_t data) override {
        if (addr < 0x2000 || addr >= 0x3F00) return false;
        const uint16_t offset = addr & 0x0FFF;
        const uint8_t quadrant = offset >> 10;
        const uint8_t source = (nametableMap >> (quadrant * 2)) & 3;
        const uint16_t index = offset & 0x03FF;
        if (source < 2) nametable[source][index] = data;
        else if (source == 2 && exMode == 0) exRam[index] = data;
        return true;
    }

    void ppuReadCycle(uint16_t addr) override {
        ppuReadThisCpuCycle = true;
        if (scanlinePending) {
            detectScanline();
            scanlinePending = false;
            repeatedAddress = 0;
        }
        const bool nametableRead = addr >= 0x2000 && addr < 0x3000;
        if (nametableRead && addr == lastPpuAddress) {
            if (repeatedAddress < 3) repeatedAddress++;
            if (repeatedAddress == 3) scanlinePending = true;
        } else {
            lastPpuAddress = addr;
            repeatedAddress = nametableRead ? 1 : 0;
        }
        if (addr >= 0x2000 && addr < 0x3000 && (addr & 0x03FF) < 0x03C0)
            lastTileOffset = addr & 0x03FF;
        ppuAccessCount++;
    }

    void clockPpu() override {
        Mapper::clockPpu();
    }

    void clock() override {
        audio.clock();
        if (ppuReadThisCpuCycle) {
            idleCpuCycles = 0;
        } else if (idleCpuCycles < 3 && ++idleCpuCycles == 3) {
            inFrame = false;
            lastPpuAddress = 0xFFFF;
            repeatedAddress = 0;
            scanlinePending = false;
            ppuAccessCount = 0;
        }
        ppuReadThisCpuCycle = false;
    }

    bool irqState() const override { return irqEnabled && irqPending; }
    void irqClear() override { irqPending = false; }
    float audioOutput() const override { return audio.output(); }
    void setAudioSettings(AudioSettings& settings) override { audio.setSettings(settings); }

private:
    bool ramWritable() const { return protectA == 2 && protectB == 1; }

    uint8_t slotRegister(uint8_t slot) const {
        switch (prgMode) {
        case 0: return prgReg[4];
        case 1: return slot < 2 ? prgReg[2] : prgReg[4];
        case 2: return slot < 2 ? prgReg[2] : prgReg[slot + 1];
        default: return prgReg[slot + 1];
        }
    }

    uint8_t prgBank(uint8_t slot) const {
        const uint8_t reg = slotRegister(slot);
        switch (prgMode) {
        case 0: return (reg & 0x7C) | slot;
        case 1: return (reg & 0x7E) | (slot & 1);
        case 2: return slot < 2 ? ((reg & 0x7E) | (slot & 1)) : (reg & 0x7F);
        default: return reg & 0x7F;
        }
    }

    bool ramBank(uint16_t addr, uint8_t& bank) const {
        if (addr >= 0x6000 && addr < 0x8000) {
            bank = prgReg[0] & 7;
            return true;
        }
        if (addr < 0x8000 || addr >= 0xE000) return false;
        const uint8_t slot = (addr - 0x8000) >> 13;
        const uint8_t reg = slotRegister(slot);
        if (reg & 0x80) return false;
        bank = prgBank(slot) & 7;
        return true;
    }

    uint32_t chrBank(uint16_t addr) const {
        const uint8_t slot = addr >> 10;
        if (splitActive && useBackgroundSet())
            return (uint32_t)splitBank * 4 + (slot & 3);
        if (exMode == 1 && useBackgroundSet()) {
            const uint16_t value = ((uint16_t)chrHigh << 6)
                | (exRam[lastTileOffset & 0x03FF] & 0x3F);
            return (uint32_t)value * 4 + (slot & 3);
        }
        const uint8_t mode = chrMode & 3;
        const uint8_t group = 8 >> mode;
        uint16_t value;
        if (useBackgroundSet()) {
            uint8_t index;
            if (mode < 2) index = 3;
            else if (mode == 2) index = (slot >> 1) & 1 ? 3 : 1;
            else index = slot & 3;
            value = chrB[index];
        } else {
            uint8_t index;
            if (mode == 0) index = 7;
            else if (mode == 1) index = slot < 4 ? 3 : 7;
            else if (mode == 2) index = (slot & 6) | 1;
            else index = slot;
            value = chrA[index];
        }
        return (uint32_t)value * group + (slot & (group - 1));
    }

    bool useBackgroundSet() const {
        if (ppuAccessCount >= 128 && ppuAccessCount < 160) return false;
        return lastChrSetB;
    }

    bool isSplitTile(uint8_t tile) const {
        if (!(splitControl & 0x80) || exMode >= 2 || tile >= 32) return false;
        const uint8_t boundary = splitControl & 0x1F;
        return splitControl & 0x40 ? tile >= boundary : tile < boundary;
    }

    void detectScanline() {
        ppuAccessCount = 0;
        if (!inFrame) {
            inFrame = true;
            irqCounter = 0;
            irqPending = false;
            return;
        }
        irqCounter++;
        if (irqCounter == irqTarget && irqTarget != 0) irqPending = true;
    }

    void writeRegister(uint16_t addr, uint8_t data) {
        if (addr >= 0x5000 && addr <= 0x5015) {
            audio.write(addr, data);
            return;
        }
        switch (addr) {
        case 0x5100: prgMode = data & 3; break;
        case 0x5101: chrMode = data & 3; break;
        case 0x5102: protectA = data & 3; break;
        case 0x5103: protectB = data & 3; break;
        case 0x5104: exMode = data & 3; break;
        case 0x5105: nametableMap = data; break;
        case 0x5106: fillTile = data; break;
        case 0x5107: fillAttribute = data & 3; break;
        case 0x5113: case 0x5114: case 0x5115: case 0x5116: case 0x5117:
            prgReg[addr - 0x5113] = data;
            break;
        case 0x5130: chrHigh = data & 3; break;
        case 0x5200: splitControl = data; break;
        case 0x5201: splitScroll = data; break;
        case 0x5202: splitBank = data; break;
        case 0x5203: irqTarget = data; break;
        case 0x5204: irqEnabled = (data & 0x80) != 0; break;
        case 0x5205: multiplierA = data; break;
        case 0x5206: multiplierB = data; break;
        default:
            if (addr >= 0x5120 && addr <= 0x5127) {
                chrA[addr - 0x5120] = ((uint16_t)chrHigh << 8) | data;
                lastChrSetB = false;
            } else if (addr >= 0x5128 && addr <= 0x512B) {
                chrB[addr - 0x5128] = ((uint16_t)chrHigh << 8) | data;
                lastChrSetB = true;
            } else if (addr >= 0x5C00 && addr <= 0x5FFF) {
                const uint16_t index = addr & 0x03FF;
                if (exMode == 2) exRam[index] = data;
                else if (exMode < 2) exRam[index] = inFrame ? data : 0;
            }
            break;
        }
    }

    uint8_t prgMode = 3;
    uint8_t chrMode = 3;
    uint8_t protectA = 0;
    uint8_t protectB = 0;
    uint8_t exMode = 0;
    uint8_t nametableMap = 0;
    uint8_t fillTile = 0;
    uint8_t fillAttribute = 0;
    std::array<uint8_t, 5> prgReg{};
    std::array<uint16_t, 8> chrA{};
    std::array<uint16_t, 4> chrB{};
    uint8_t chrHigh = 0;
    bool lastChrSetB = false;
    uint8_t splitControl = 0;
    uint8_t splitScroll = 0;
    uint8_t splitBank = 0;
    uint8_t irqTarget = 0;
    uint8_t irqCounter = 0;
    bool irqEnabled = false;
    bool irqPending = false;
    bool inFrame = false;
    uint8_t multiplierA = 0;
    uint8_t multiplierB = 0;
    std::array<uint8_t, 65536> prgRam{};
    std::array<uint8_t, 1024> exRam{};
    std::array<std::array<uint8_t, 1024>, 2> nametable{};
    uint16_t lastPpuAddress = 0xFFFF;
    uint8_t repeatedAddress = 0;
    bool scanlinePending = false;
    uint16_t ppuAccessCount = 0;
    bool ppuReadThisCpuCycle = false;
    uint8_t idleCpuCycles = 0;
    uint16_t lastTileOffset = 0;
    bool splitActive = false;
    uint8_t splitFineY = 0;
    MMC5Audio audio;
    const char* name() const override { return "MMC5"; }
};

REGISTER_MAPPER(5, Mapper005)
