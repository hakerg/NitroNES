#pragma once
#include "NESHeadlessSystem.h"
#include "CycleTracer.h"
#include <optional>

class TracedNESHeadlessSystem : public NESHeadlessSystem {
public:
    explicit TracedNESHeadlessSystem(const std::string& path)
        : NESHeadlessSystem(path) {}

    void attachTracer(CycleTracer::Config cfg) {
        tracer.emplace(std::move(cfg));
        cycleCount = 0;
    }

    bool traceIsDone() const { return tracer && tracer->isDone(); }

protected:
    void onPostStep() override {
        if (!tracer) return;
        auto& cpu = a2a03.getCPU();
        auto& dma = a2a03.getDMA();

        CycleSnapshot snap;
        snap.cycle       = cycleCount++;
        snap.pc          = cpu.PC;
        snap.a           = cpu.A;
        snap.x           = cpu.X;
        snap.y           = cpu.Y;
        snap.sp          = cpu.S;
        snap.p           = cpu.P;
        snap.busAddr     = a2a03.getAddr();
        snap.busData     = a2a03.getBusData();
        snap.cpuRead     = cpu.isRead();
        snap.dmaActive   = dma.overridesAddr();
        snap.dmaAddr     = dma.getAddr();
        snap.dmcPhase    = dma.dmcPhaseId();
        snap.oamPhase    = dma.oamPhaseId();
        snap.dmaAction   = dma.actionId();
        snap.ppuScanline = ppu.getScanline();
        snap.ppuCycle    = ppu.getCycle();
        snap.nmi         = ppu.nmiLineLow();
        snap.irq         = !a2a03.pollIRQ();
        tracer->push(snap);
    }

private:
    std::optional<CycleTracer> tracer;
    uint64_t cycleCount = 0;
};







