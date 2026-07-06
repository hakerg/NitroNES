#pragma once
#include <cstdint>
#include <cstdio>
#include "DelayedPin.h"
#include "Tracer.h"

class IDMA {
public:
    virtual ~IDMA() = default;
    virtual bool dmcReadyForFetch() = 0;
    virtual bool dmcHasBytesRemaining() = 0;
    virtual bool dmcDMAHaltOnPut() = 0;
    virtual bool pollIsRead() = 0;
    virtual uint8_t dmaReadData() = 0;
    virtual uint16_t getDMCSampleAddress() = 0;
    virtual void writeOAMData(uint8_t data) = 0;
    virtual void loadDMCSample(uint8_t data) = 0;
};

class DMA {
public:
    DMA(IDMA& idma) : idma(idma) {}

    void reset() {
        rdy = true;
        oamPhase = OAMPhase::Idle;
        dmcPhase = DMCPhase::Idle;
        oamHasByte = false;
        action = Action::None;
        dmcHaltOnPut = false;
        dmcHaltArmed = false;
        dmcAbortPending = false;
        hadBytesRecently.force(false);
    }

    void clockPhi1(bool isPutCycle) {
        action = Action::None;
        const bool cpuRead = idma.pollIsRead();
        const bool getCycle = !isPutCycle;

        if (idma.dmcHasBytesRemaining()) {
            hadBytesRecently.force(true);
        } else if (hadBytesRecently.getTarget()) {
            hadBytesRecently.set(false, 4);
        }
        hadBytesRecently.tick();

        if (dmcPhase == DMCPhase::Idle) {
            if (hadBytesRecently.get() && idma.dmcReadyForFetch()) {
                dmcPhase = DMCPhase::Halt;
                dmcHaltOnPut = idma.dmcDMAHaltOnPut();
                dmcHaltArmed = false;
                dmcAbortPending = true;
            }
        } else if (dmcAbortPending) {
            dmcAbortPending = false;
            if (!hadBytesRecently.getTap(1)) {
                dmcPhase = DMCPhase::Idle;
            }
        }

        switch (dmcPhase) {
            case DMCPhase::Idle:
                break;
            case DMCPhase::Halt:
                if (!dmcHaltArmed && (isPutCycle == dmcHaltOnPut))
                    dmcHaltArmed = true;
                if (dmcHaltArmed && cpuRead)
                    dmcPhase = DMCPhase::Dummy;
                break;
            case DMCPhase::Dummy:
                dmcPhase = DMCPhase::Read;
                break;
            case DMCPhase::Read:
                if (getCycle)
                    action = Action::DMCGet;
                break;
        }

        switch (oamPhase) {
            case OAMPhase::Idle:
                break;
            case OAMPhase::Halt:
                if (cpuRead) { oamPhase = OAMPhase::Transfer; oamHasByte = false; }
                break;
            case OAMPhase::Transfer:
                if (action == Action::None) {
                    if (!oamHasByte) {
                        if (getCycle) action = Action::OAMGet;
                    } else if (isPutCycle) {
                        action = Action::OAMPut;
                    }
                }
                break;
        }

        const bool halted = (oamPhase == OAMPhase::Transfer)
                         || (dmcPhase == DMCPhase::Dummy)
                         || (dmcPhase == DMCPhase::Read);
        rdy = !halted;
    }

    void clockPhi2() {
        switch (action) {
            case Action::None:
                break;
            case Action::DMCGet:
                idma.loadDMCSample(idma.dmaReadData());
                dmcPhase = DMCPhase::Idle;
                break;
            case Action::OAMGet:
                oamBuffer = idma.dmaReadData();
                oamHasByte = true;
                break;
            case Action::OAMPut:
                idma.writeOAMData(oamBuffer);
                oamHasByte = false;
                if ((oamAddr & 0xFF) == 0xFF) oamPhase = OAMPhase::Idle;
                else                          oamAddr++;
                break;
        }
        emitTrace();
    }

    uint16_t getAddr() {
        if (action == Action::DMCGet) return idma.getDMCSampleAddress();
        if (action == Action::OAMGet) return oamAddr;
        if (action == Action::OAMPut) return 0x2004;
        return 0;
    }

    void write4014(uint8_t data) {
        oamAddr  = (uint16_t)data << 8;
        oamPhase = OAMPhase::Halt;
    }

    bool overridesAddr() const { return action != Action::None; }
    bool getRDYOut()     const { return rdy; }

    void setTracer(Tracer* t) { tracer = t; }

private:
    enum class OAMPhase { Idle, Halt, Transfer };
    enum class DMCPhase { Idle, Halt, Dummy, Read };
    enum class Action   { None, OAMGet, OAMPut, DMCGet };

    void emitTrace() {
        if (!tracer || !tracer->dma) return;
        static const char* dmcPh[] = { "Idle", "Halt", "Dummy", "Read" };
        static const char* oamPh[] = { "Idle", "Halt", "Xfer" };
        static const char* acts[]  = { "None", "OAMGet", "OAMPut", "DMCGet" };
        char buf[160];
        int n = std::snprintf(buf, sizeof(buf), "DMA:%s/%s %s @%04X hb=%d",
                      oamPh[(int)oamPhase & 3],
                      dmcPh[(int)dmcPhase & 3],
                      acts[(int)action    & 3],
                      (unsigned)getAddr(),
                      (int)hadBytesRecently.get());
        if (hadBytesRecently.isPending())
            std::snprintf(buf + n, sizeof(buf) - n, "->%d(%d)",
                          (int)hadBytesRecently.getTarget(), hadBytesRecently.getDelay());
        tracer->appendDma(buf);
    }

    IDMA& idma;
    Tracer* tracer = nullptr;
    bool rdy = true;

    OAMPhase oamPhase = OAMPhase::Idle;
    uint16_t oamAddr  = 0;
    uint8_t  oamBuffer = 0;
    bool     oamHasByte = false;

    DMCPhase dmcPhase = DMCPhase::Idle;
    bool     dmcHaltOnPut = false;
    bool     dmcHaltArmed = false;
    bool     dmcAbortPending = false;
    DelayedPin<bool> hadBytesRecently{false};

    Action action = Action::None;
};