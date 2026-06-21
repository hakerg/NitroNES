#pragma once
#include <array>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <string>

struct CycleSnapshot {
    uint64_t cycle      = 0;
    uint16_t busAddr    = 0;
    uint16_t dmaAddr    = 0;
    uint16_t pc         = 0;
    uint8_t  a = 0, x = 0, y = 0, sp = 0, p = 0;
    uint8_t  busData    = 0;
    bool     cpuRead    = false;
    bool     dmaActive  = false;
    int      dmcPhase   = 0;
    int      oamPhase   = 0;
    int      dmaAction  = 0;
    int16_t  ppuScanline = 0;
    int16_t  ppuCycle    = 0;
    bool     nmi        = false;
    bool     irq        = false;
};

class CycleTracer {
public:
    enum class TriggerKind { None, OAMDMAStart, DMCDMAStart, NMIEdge, PCValue, AddrWrite };

    struct Config {
        TriggerKind triggerKind = TriggerKind::None;
        uint16_t    triggerAddr = 0;
        int         preCycles   = 300;
        int         postCycles  = 300;
        std::string outFile     = "trace.tsv";
    };

    explicit CycleTracer(Config cfg) : cfg(std::move(cfg)) {}

    bool isDone() const { return captured; }

    void push(const CycleSnapshot& snap) {
        ringBuf[head] = snap;
        head = (head + 1) % RING_SIZE;
        if (ringFill < RING_SIZE) ringFill++;

        if (capturing) {
            writeLine(snap);
            if (--postLeft == 0) stopCapture();
        } else if (!captured && shouldTrigger(snap)) {
            startCapture();
            if (fp) {
                writeLine(snap);
                if (--postLeft == 0) stopCapture();
            }
        }

        prevSnap = snap;
    }

private:
    static constexpr int RING_SIZE = 1024;

    Config cfg;
    std::array<CycleSnapshot, RING_SIZE> ringBuf{};
    CycleSnapshot prevSnap{};
    int      head     = 0;
    int      ringFill = 0;
    bool     capturing = false;
    bool     captured  = false;
    int      postLeft  = 0;
    FILE*    fp        = nullptr;

    bool shouldTrigger(const CycleSnapshot& s) const {
        switch (cfg.triggerKind) {
        case TriggerKind::None:        return false;
        case TriggerKind::OAMDMAStart: return !s.cpuRead && s.busAddr == 0x4014;
        case TriggerKind::DMCDMAStart: return s.dmcPhase != 0 && prevSnap.dmcPhase == 0;
        case TriggerKind::NMIEdge:     return s.nmi && !prevSnap.nmi;
        case TriggerKind::PCValue:     return s.pc == cfg.triggerAddr;
        case TriggerKind::AddrWrite:   return !s.cpuRead && s.busAddr == cfg.triggerAddr;
        }
        return false;
    }

    void startCapture() {
        captured = true;  // mark as triggered regardless of file open success
        fp = fopen(cfg.outFile.c_str(), "w");
        if (!fp) {
            fprintf(stderr, "[CycleTracer] fopen failed for: %s\n", cfg.outFile.c_str());
            return;
        }
        capturing = true;
        captured  = true;
        postLeft  = cfg.postCycles;
        fprintf(fp, "CYC\tPC\tA\tX\tY\tSP\tP\tBUS_ADDR\tBUS_DATA\tRW\tDMA_ACT\tDMC_PH\tOAM_PH\tACTION\tDMA_ADDR\tPPU_SL\tPPU_CY\tNMI\tIRQ\n");
        int count = std::min(ringFill, cfg.preCycles);
        int start = (head - count + RING_SIZE) % RING_SIZE;
        for (int i = 0; i < count; i++)
            writeLine(ringBuf[(start + i) % RING_SIZE]);
    }

    void stopCapture() {
        capturing = false;
        if (fp) { fclose(fp); fp = nullptr; }
    }

    void writeLine(const CycleSnapshot& s) {
        static constexpr const char* dmcPh[] = { "Idle", "Halt", "Dummy", "Read" };
        static constexpr const char* oamPh[] = { "Idle", "Halt", "Xfer" };
        static constexpr const char* acts[]  = { "None", "OAMGet", "OAMPut", "DMCGet" };
        fprintf(fp,
            "%llu\t%04X\t%02X\t%02X\t%02X\t%02X\t%02X\t%04X\t%02X\t%c\t%d\t%s\t%s\t%s\t%04X\t%d\t%d\t%d\t%d\n",
            (unsigned long long)s.cycle,
            (unsigned)s.pc, (unsigned)s.a, (unsigned)s.x,
            (unsigned)s.y,  (unsigned)s.sp,(unsigned)s.p,
            (unsigned)s.busAddr, (unsigned)s.busData,
            s.cpuRead ? 'R' : 'W',
            s.dmaActive ? 1 : 0,
            dmcPh[s.dmcPhase & 3],
            oamPh[s.oamPhase & 3],
            acts[s.dmaAction & 3],
            (unsigned)s.dmaAddr,
            (int)s.ppuScanline, (int)s.ppuCycle,
            s.nmi ? 1 : 0, s.irq ? 1 : 0);
    }
};



