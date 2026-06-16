#pragma once
#include "CPU6502.h"
#include "APU.h"

class A2A03 : public ICPUBus {
public:
    explicit A2A03(ICPUBus& motherboardBus)
        : cpu(*this), motherboard(motherboardBus) {}

    void reset() {
        cpu.reset();
        apu.reset();
        dma.state = DMA::State::Idle;
        dma.oamPending = false;
        dma.dmcPending = false;
        dataBus = 0x00;
    }

    void clockPhi1() {
        apu.clock(isAPUCycle);

        if (dma.active()) {
            cpu.onRdyLow();
            stepDMA(isAPUCycle);
        } else {
            cpu.clockPhi1();
        }

        if (apu.dmcNeedsSample() && !dma.dmcPending) {
            dma.dmcPending = true;
            dma.dmcAddr = apu.dmcSampleAddress();
        }

        isAPUCycle = !isAPUCycle;
    }

    void clockPhi2() {
        if (!dma.active()) {
            cpu.clockPhi2();
        }
    }

    CPU6502& getCPU() { return cpu; }
    APU& getAPU() { return apu; }

    uint8_t getDataBus() const { return dataBus; }

    uint8_t cpuRead(uint16_t addr) override {
        lastCpuAddr = addr;

        if (addr == 0x4015) {
            dataBus = apu.cpuRead(addr, dataBus);
        } else if (addr >= 0x4000 && addr <= 0x4013) {
        } else if (addr == 0x4014) {
        } else {
            dataBus = motherboard.cpuRead(addr);
        }

        return dataBus;
    }

    void cpuWrite(uint16_t addr, uint8_t data) override {
        lastCpuAddr = addr;
        dataBus = data;

        motherboard.cpuWrite(addr, data);

        if (addr == 0x4014) {
            dma.oamPending = true;
            dma.oamPage = (uint16_t)data << 8;
        } else if (addr >= 0x4000 && addr <= 0x4017) {
            apu.cpuWrite(addr, data, isAPUCycle);
        }
    }

    void cpuIrqAck() override {
        motherboard.cpuIrqAck();
    }

    bool pollNMI() override {
        return motherboard.pollNMI();
    }

    bool pollIRQ() override {
        return motherboard.pollIRQ();
    }

private:
    CPU6502 cpu;
    APU apu;
    ICPUBus& motherboard;

    uint8_t dataBus = 0x00;
    uint16_t lastCpuAddr = 0x0000;
    bool isAPUCycle = false;

    struct DMA {
        bool oamPending = false;
        uint16_t oamPage = 0;
        bool dmcPending = false;
        uint16_t dmcAddr = 0;
        uint16_t oamOffset = 0;
        uint8_t oamData = 0;
        enum class State {
            Idle, Halt_DMC, Align_DMC, Dummy_DMC, Read_DMC,
            Halt_OAM, Align_OAM, Read_OAM, Write_OAM
        } state = State::Idle;
        State returnState = State::Idle;

        bool active() const { return state != State::Idle || oamPending || dmcPending; }
    } dma;

    void stepDMA(bool isPutCycle) {
        if (dma.state == DMA::State::Idle) {
            if (dma.dmcPending) dma.state = DMA::State::Halt_DMC;
            else if (dma.oamPending) dma.state = DMA::State::Halt_OAM;
        } else if (dma.dmcPending && dma.state >= DMA::State::Halt_OAM) {
            dma.returnState = dma.state;
            dma.state = DMA::State::Halt_DMC;
        }

        switch (dma.state) {
            case DMA::State::Halt_DMC:
                motherboard.cpuRead(lastCpuAddr);
                dma.state = isPutCycle ? DMA::State::Align_DMC : DMA::State::Dummy_DMC;
                break;
            case DMA::State::Align_DMC:
                motherboard.cpuRead(lastCpuAddr);
                dma.state = DMA::State::Dummy_DMC;
                break;
            case DMA::State::Dummy_DMC:
                motherboard.cpuRead(lastCpuAddr);
                dma.state = DMA::State::Read_DMC;
                break;
            case DMA::State::Read_DMC: {
                uint8_t b = motherboard.cpuRead(dma.dmcAddr);
                apu.loadDMCSample(b);
                dma.dmcPending = false;
                if (dma.returnState != DMA::State::Idle) {
                    dma.state = dma.returnState;
                    dma.returnState = DMA::State::Idle;
                } else if (dma.oamPending) {
                    dma.state = DMA::State::Halt_OAM;
                } else {
                    dma.state = DMA::State::Idle;
                }
                break;
            }

            case DMA::State::Halt_OAM:
                motherboard.cpuRead(lastCpuAddr);
                dma.state = (!isPutCycle) ? DMA::State::Align_OAM : DMA::State::Read_OAM;
                break;
            case DMA::State::Align_OAM:
                motherboard.cpuRead(lastCpuAddr);
                dma.state = DMA::State::Read_OAM;
                break;
            case DMA::State::Read_OAM:
                dma.oamData = motherboard.cpuRead(dma.oamPage + dma.oamOffset);
                dma.state = DMA::State::Write_OAM;
                break;
            case DMA::State::Write_OAM: {
                motherboard.cpuWrite(0x2004, dma.oamData);
                dma.oamOffset++;
                if (dma.oamOffset == 256) {
                    dma.oamOffset = 0;
                    dma.oamPending = false;
                    dma.state = DMA::State::Idle;
                } else {
                    dma.state = DMA::State::Read_OAM;
                }
                break;
            }
            case DMA::State::Idle:
                break;
        }
    }
};