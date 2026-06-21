#pragma once
#include <cstdint>

class IDMA {
public:
    virtual ~IDMA() = default;
    virtual bool isDMCSampleNeeded() = 0;
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
    }

    void clockPhi1(bool isPutCycle) {
        action = Action::None;
        const bool cpuRead = idma.pollIsRead();
        const bool getCycle = !isPutCycle;

        if (dmcPhase == DMCPhase::Idle && idma.isDMCSampleNeeded()) {
            dmcPhase = DMCPhase::Halt;
            // Capture the phase this DMC DMA must halt on: load DMAs halt on a get
            // cycle (3 cycles total), reload DMAs halt on a put cycle (4 cycles).
            dmcHaltOnPut = idma.dmcDMAHaltOnPut();
            dmcHaltArmed = false;
        }

        switch (dmcPhase) {
            case DMCPhase::Idle:
                break;
            case DMCPhase::Halt:
                // The first halt attempt is scheduled for a specific phase (get
                // for load, put for reload). Once that scheduled cycle is reached
                // the halt is attempted every CPU cycle and only fails on write
                // cycles, retrying on the next cycle (nes_specs/dma.txt, dma forum.txt).
                // So a write on the scheduled cycle delays the halt to the next
                // (opposite-phase) read, which skips the alignment cycle and makes
                // the DMA take 3 cycles instead of 4.
                if (!dmcHaltArmed && (isPutCycle == dmcHaltOnPut))
                    dmcHaltArmed = true;
                if (dmcHaltArmed && cpuRead)
                    dmcPhase = DMCPhase::Dummy;
                else if (!idma.isDMCSampleNeeded())
                    // Playback was stopped before the DMA could halt: with the halt
                    // delayed by a write cycle, the aborted DMA does not occur at all.
                    dmcPhase = DMCPhase::Idle;
                break;
            case DMCPhase::Dummy:
                if (!idma.isDMCSampleNeeded())
                    // Playback was stopped after the halt cycle: the DMA is aborted
                    // after that single cycle (nes_specs/dma.txt "Bugs").
                    dmcPhase = DMCPhase::Idle;
                else
                    dmcPhase = DMCPhase::Read;
                break;
            case DMCPhase::Read:
                if (!idma.isDMCSampleNeeded())
                    dmcPhase = DMCPhase::Idle;
                else if (getCycle)
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

    bool dmcIsActive()  const { return dmcPhase != DMCPhase::Idle; }
    bool oamIsActive()  const { return oamPhase != OAMPhase::Idle; }
    bool actionIsDMCGet() const { return action == Action::DMCGet; }
    int  dmcPhaseId()   const { return (int)dmcPhase; }
    int  oamPhaseId()   const { return (int)oamPhase; }
    int  actionId()     const { return (int)action; }

private:
    enum class OAMPhase { Idle, Halt, Transfer };
    enum class DMCPhase { Idle, Halt, Dummy, Read };
    enum class Action   { None, OAMGet, OAMPut, DMCGet };

    IDMA& idma;
    bool rdy = true;

    OAMPhase oamPhase = OAMPhase::Idle;
    uint16_t oamAddr  = 0;
    uint8_t  oamBuffer = 0;
    bool     oamHasByte = false;

    DMCPhase dmcPhase = DMCPhase::Idle;
    bool     dmcHaltOnPut = false;
    bool     dmcHaltArmed = false;

    Action action = Action::None;
};