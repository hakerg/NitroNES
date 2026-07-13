#pragma once
#include "APU.h"
#include "CPU6502.h"
#include "DMA.h"

class NESCoreBase;

class IA2A03 {
public:
    virtual ~IA2A03() = default;
    virtual bool    pollNMI()                                    = 0;
    virtual bool    irqAsserted()                                = 0;
    virtual uint8_t a2a03ReadData(uint16_t addr)                 = 0;
    virtual uint8_t a2a03ReadDataExternal(uint16_t addr)         = 0;
    virtual void    a2a03WriteData(uint16_t addr, uint8_t data)  = 0;
    virtual void    latchControllers()                           {}
};

class A2A03 : public ICPUBus, public IDMA {
public:
    explicit A2A03(IA2A03& core, AudioSettings& audioSettings)
        : cpu(*this), apu(audioSettings), dma(*this), core(core) {}

    bool isControllerStrobeActive() const { return controllerStrobe; }

    void reset() {
        cpu.reset();
        apu.reset();
        dma.reset();

        busData = 0;
        isAPUPutCycle = false;
        internalBus = 0;
        controllerStrobe = false;
    }

    void clockPhi1() {
        apu.clock(isAPUPutCycle);
        cpu.clockPhi1();
        dma.clockPhi1(isAPUPutCycle);
        apu.emitTrace();
    }

    void clockPhi2() {
        if (!cpu.isRead()) {
            writeData(cpu.getWriteData());
        }
        dma.clockPhi2();
        cpu.clockPhi2();

        if (controllerStrobe && !isAPUPutCycle) {
            core.latchControllers();
        }

        isAPUPutCycle = !isAPUPutCycle;
    }

    CPU6502& getCPU() { return cpu; }
    APU& getAPU() { return apu; }
    DMA& getDMA() { return dma; }
    uint8_t getBusData() { return busData; }

    void writeAPU(uint16_t addr, uint8_t data) { apu.writeData(addr, data, isAPUPutCycle); }

    void setTracer(Tracer* t) { cpu.setTracer(t); dma.setTracer(t); apu.setTracer(t); }

    uint8_t cpuReadData() override {
        if (dma.overridesAddr()) return busData;
        return readData();
    }

    bool pollNMI() override { return core.pollNMI(); }
    bool pollIRQ() override { return !(apu.irqAsserted() || core.irqAsserted()); }
    bool pollRDY() override { return dma.getRDYOut(); }
    bool isReadOverridden() override { return dma.overridesAddr(); }
    bool dmcReadyForFetch() override { return apu.dmcReadyForFetch(); }
    bool dmcHasBytesRemaining() override { return apu.dmcHasBytesRemaining(); }
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

private:
    friend class NESCoreBase;

    uint16_t getAddr() { return dma.overridesAddr() ? dma.getAddr() : cpu.getAddr(); }

    void writeExternal(uint16_t addr, uint8_t data) {
        writeData(addr, data);
    }

    uint8_t readData() {
        if (dma.overridesAddr()) return readDMA();

        uint16_t addr = cpu.getAddr();
        if (addr >= 0x4000 && addr <= 0x4014) { internalBus = busData; return busData; }
        if (addr == 0x4015) {
            internalBus = apu.readData(addr, internalBus);
            return internalBus;
        }
        busData = core.a2a03ReadData(addr);
        internalBus = busData;
        return busData;
    }

    uint8_t readDMA() {
        const uint16_t dmaAddr  = dma.getAddr();
        const uint16_t coreAddr = cpu.getAddr();
        const bool regActive = (coreAddr >= 0x4000 && coreAddr <= 0x401F);
        const uint16_t reg = 0x4000 | (dmaAddr & 0x001F);

        if (regActive && (reg == 0x4016 || reg == 0x4017)) {
            const uint8_t ctrl = core.a2a03ReadData(reg);
            const uint8_t ext  = core.a2a03ReadDataExternal(dmaAddr);
            busData = (ext & 0xE0) | (ctrl & 0x1F);
            internalBus = busData;
            return busData;
        }

        busData = readExternal(dmaAddr);

        if (regActive && reg == 0x4015) {
            internalBus = apu.readData(0x4015, internalBus);
            return internalBus;
        }

        internalBus = busData;
        return busData;
    }

    uint8_t readExternal(uint16_t addr) {
        if (addr >= 0x4000 && addr <= 0x401F) return busData;
        return core.a2a03ReadData(addr);
    }

    void writeData(uint8_t data) {
        writeData(getAddr(), data);
    }

    void writeData(uint16_t addr, uint8_t data) {
        busData = data;
        internalBus = data;

        core.a2a03WriteData(addr, data);

        if (addr == 0x4016) {
            controllerStrobe = (data & 0x01) != 0;
        }

        if (addr == 0x4014) {
            dma.write4014(data);
        }
    }

    CPU6502 cpu;
    APU apu;
    DMA dma;
    IA2A03& core;

    uint8_t busData = 0;
    bool isAPUPutCycle = false;
    uint8_t internalBus = 0;
    bool controllerStrobe = false;
};