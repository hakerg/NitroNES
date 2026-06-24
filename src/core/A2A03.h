#pragma once
#include "APU.h"
#include "CPU6502.h"
#include "DMA.h"

class IA2A03 {
public:
    virtual ~IA2A03() = default;
    virtual bool    pollNMI()                                    = 0;
    virtual bool    irqAsserted()                                = 0;
    virtual uint8_t a2a03ReadData(uint16_t addr)                 = 0;
    // Reads the external bus without disturbing the controller-port clock
    // tracking (used for the DMA value during a DMC register conflict).
    virtual uint8_t a2a03ReadDataExternal(uint16_t addr)         = 0;
    virtual void    a2a03WriteData(uint16_t addr, uint8_t data)  = 0;
};

class A2A03 : public ICPUBus, public IDMA {
public:
    explicit A2A03(IA2A03& core)
        : cpu(*this), dma(*this), core(core) {}

    void reset() {
        cpu.reset();
        apu.reset();
        dma.reset();

        busData = 0;
        isAPUPutCycle = false;
        putCycleProcessed = false;
        internalBus = 0;
    }

    void clockPhi1() {
        apu.clock(isAPUPutCycle);
        cpu.clockPhi1();
        dma.clockPhi1(isAPUPutCycle);
    }

    void clockPhi2() {
        if (!cpu.isRead()) {
            writeData(cpu.getWriteData());
        }
        dma.clockPhi2();
        cpu.clockPhi2();
        putCycleProcessed = isAPUPutCycle;
        isAPUPutCycle = !isAPUPutCycle;
    }

    CPU6502& getCPU() { return cpu; }
    APU& getAPU() { return apu; }
    DMA& getDMA() { return dma; }
    uint8_t getBusData() { return busData; }
    bool lastWasPutCycle() const { return putCycleProcessed; }

    uint8_t cpuReadData() override {
        if (dma.overridesAddr()) return busData;
        return readData();
    }

    bool pollNMI() override { return core.pollNMI(); }
    bool pollIRQ() override { return !(apu.irqAsserted() || core.irqAsserted()); }
    bool pollRDY() override { return dma.getRDYOut(); }
    bool isReadOverridden() override { return dma.overridesAddr(); }
    bool isDMCSampleNeeded() override { return apu.dmcNeedsSample(); }
    bool dmcDMAHaltOnPut()   override { return apu.dmcDMAHaltOnPut(); }
    bool pollIsRead()        override { return cpu.isRead(); }

    uint8_t dmaReadData() override {
        return readData();
    }

    uint16_t getDMCSampleAddress() override { return apu.dmcSampleAddress(); }

    void writeOAMData(uint8_t data) override {
        busData = data;
        core.a2a03WriteData(0x2004, data);
    }

    void loadDMCSample(uint8_t data) override { apu.loadDMCSample(data); }
    uint16_t getAddr() { return dma.overridesAddr() ? dma.getAddr() : cpu.getAddr(); }

    uint8_t readData() {
        if (dma.overridesAddr()) return readDMA();

        uint16_t addr = cpu.getAddr();
        if (addr >= 0x4000 && addr <= 0x4014) { internalBus = busData; return busData; }
        if (addr == 0x4015) {
            // A $4015 read drives and reads only the internal data bus; the
            // external (open) bus is left unchanged.
            internalBus = apu.readData(addr, internalBus);
            return internalBus;
        }
        busData = core.a2a03ReadData(addr);
        internalBus = busData;
        return busData;
    }

    // A DMA unit drives the external address bus, but the 2A03's internal
    // registers ($4000-$401F) are decoded from bits 4-0 of the 2A03 address bus
    // (the DMA address) combined with bits 15-5 of the 6502 core address, and only
    // activate when the 6502 core address is itself in $4000-$401F
    // (nes_specs/dma.txt "Register conflicts").
    uint8_t readDMA() {
        const uint16_t dmaAddr  = dma.getAddr();
        const uint16_t coreAddr = cpu.getAddr();
        const bool regActive = (coreAddr >= 0x4000 && coreAddr <= 0x401F);
        const uint16_t reg = 0x4000 | (dmaAddr & 0x001F);

        if (regActive && (reg == 0x4016 || reg == 0x4017) && dma.actionIsDMCGet()) {
            // The controller is decoded at the combined address ($4016/$4017), so it
            // is a read of that port. Read it at the decoded address (which keeps it
            // consecutive with the surrounding CPU reads of the same port), then
            // fetch the external DMA value for the open-bus bits D5-D7 without
            // disturbing the controller clock tracking.
            const uint8_t ctrl = core.a2a03ReadData(reg);
            const uint8_t ext  = core.a2a03ReadDataExternal(dmaAddr);
            busData = (ext & 0xE0) | (ctrl & 0x1F);
            internalBus = busData;
            return busData;
        }

        busData = readExternal(dmaAddr);

        if (regActive && reg == 0x4015) {
            // The 2A03 reads $4015 internally: this updates only the internal data
            // bus, while the external bus still carries the (ignored) DMA value.
            internalBus = apu.readData(0x4015, internalBus);
            return busData;
        }

        internalBus = busData;
        return busData;
    }

    uint8_t readExternal(uint16_t addr) {
        if (addr >= 0x4000 && addr <= 0x401F) return busData;
        return core.a2a03ReadData(addr);
    }

    void writeData(uint8_t data) {
        busData = data;
        internalBus = data;
        uint16_t addr = getAddr();
        core.a2a03WriteData(addr, data);

        if (addr == 0x4014) {
            dma.write4014(data);
        } else if (addr >= 0x4000 && addr <= 0x4017) {
            apu.writeData(addr, data, isAPUPutCycle);
        }
    }

private:
    CPU6502 cpu;
    APU apu;
    DMA dma;
    IA2A03& core;

    uint8_t busData = 0;
    bool isAPUPutCycle = false;
    bool putCycleProcessed = false;
    uint8_t internalBus = 0;
};