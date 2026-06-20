#pragma once
#include "APU.h"
#include "CPU6502.h"
#include "DMA.h"

// Define ACCURACY_DMA_TRACE (e.g. add target_compile_definitions or -DACCURACY_DMA_TRACE)
// to print a per-cycle trace of every DMC DMA and the few cycles around it.
#if defined(ACCURACY_DMA_TRACE)
#include <cstdio>
#endif

class IA2A03 {
public:
    virtual ~IA2A03() = default;
    virtual bool    pollNMI()                                    = 0;
    virtual bool    irqAsserted()                                = 0;
    virtual uint8_t a2a03ReadData(uint16_t addr)                 = 0;
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
#if defined(ACCURACY_DMA_TRACE)
        traceDMC();
#endif
        isAPUPutCycle = !isAPUPutCycle;
    }

    CPU6502& getCPU() { return cpu; }
    APU& getAPU() { return apu; }
    uint8_t getBusData() { return busData; }

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
        uint16_t addr = getAddr();
        if (addr >= 0x4000 && addr <= 0x4014) return busData;
        if (addr == 0x4015) return apu.readData(addr, busData);
        busData = core.a2a03ReadData(addr);
        return busData;
    }

    void writeData(uint8_t data) {
        busData = data;
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

#if defined(ACCURACY_DMA_TRACE)
    unsigned long long traceCycle = 0;
    int dmcTraceTail = 0;
    int traceBudget = 800;
    bool oamSeen = false;
    bool oamTraceDone = false;
    int  oamTraceTail = 0;

    void traceDMC() {
        ++traceCycle;
        const bool oamActive = dma.oamIsActive();
        const bool dmcActive = dma.dmcIsActive();
        // Capture the first full OAM DMA (with any DMC interruption) plus a few
        // cycles on each side, to compare the cadence against hardware traces.
        if (oamTraceDone) return;
        if (!oamActive && oamTraceTail == 0) {
            if (!oamSeen) return;        // not started yet
            oamTraceDone = true;         // finished the window
            return;
        }
        if (oamActive) { oamSeen = true; oamTraceTail = 4; }
        else           { --oamTraceTail; }

        static const char* phase[] = { "Idle", "Halt", "Dummy", "Read" };
        static const char* act[]   = { "None", "OAMGet", "OAMPut", "DMCGet" };
        std::printf("[OAM] cyc=%llu put=%d cpuRead=%d cpuAddr=$%04X "
                    "override=%d dmaAddr=$%04X bus=$%02X dmcPhase=%s action=%s rdy=%d oam=%d\n",
                    traceCycle, isAPUPutCycle ? 1 : 0, cpu.isRead() ? 1 : 0, cpu.getAddr(),
                    dma.overridesAddr() ? 1 : 0, dma.getAddr(), busData,
                    phase[dma.dmcPhaseId()], act[dma.actionId()], dma.getRDYOut() ? 1 : 0,
                    oamActive ? 1 : 0);
        (void)dmcActive;
    }
#endif
};