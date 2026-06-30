#pragma once
#include <cstdint>
#include <cstdio>
#include <string>
#include "Tracer.h"

class ICPUBus {
public:
    virtual ~ICPUBus() = default;
    virtual uint8_t cpuReadData() = 0;
    virtual bool pollNMI() = 0;
    virtual bool pollIRQ() = 0;
    virtual bool pollRDY() = 0;
    virtual bool isReadOverridden() = 0;
};

class CPU6502 {
public:
    static constexpr uint8_t FLAG_C = 0x01;
    static constexpr uint8_t FLAG_Z = 0x02;
    static constexpr uint8_t FLAG_I = 0x04;
    static constexpr uint8_t FLAG_D = 0x08;
    static constexpr uint8_t FLAG_B = 0x10;
    static constexpr uint8_t FLAG_U = 0x20;
    static constexpr uint8_t FLAG_V = 0x40;
    static constexpr uint8_t FLAG_N = 0x80;

    CPU6502(ICPUBus& busInterface) : bus(busInterface) {
        reset();
    }

    void reset() {
        P |= FLAG_I;

        nmiPending = false;
        interruptPending = false;
        currentInt = IntKind::None;

        nmiLevel = true;
        irqLevel = true;
        rdyLevel = true;
        skipNextPoll = false;

        irqInhibitSnapshot = true;

        nmiEdgeDetected = false;
        nmiPollSignal   = false;
        irqDetected = false;

        cycle = 0;

        currentStep = emitRead(&CPU6502::reset1, PC);
        nextOp = currentStep.next;
    }

    void jumpTo(uint16_t pc) {
        PC = pc;
        currentStep = emitRead(&CPU6502::opFetch, pc);
        nextOp = &CPU6502::opFetch;
    }

    void clockPhi1() {
        rdyLevel = bus.pollRDY();
        nmiPollSignal   = nmiEdgeDetected;
        nmiEdgeDetected = nmiPending;
        irqDetected = !irqLevel;

        const RegSnap pre{ PC, A, X, Y, S, P };

        if (isStalled()) {
            origHigh = 0xFE;
            emitTracePhi1(pre, true);
            return;
        }

        currentOp = nextOp;
        currentStep = (this->*nextOp)();
        nextOp = currentStep.next;
        emitTracePhi1(pre, false);
    }

    void clockPhi2() {
        if (currentStep.isRead && !bus.isReadOverridden()) {
            fetched = bus.cpuReadData();
        }

        bool currentNMILevel = bus.pollNMI();
        if (!currentNMILevel && nmiLevel) {
            nmiPending = true;
        }
        nmiLevel = currentNMILevel;
        irqLevel = bus.pollIRQ();

        emitTracePhi2();
        cycle++;
    }

    uint16_t getAddr()      { return currentStep.busAddr; }
    uint8_t  getWriteData() { return currentStep.writeData; }
    bool     isRead()       { return currentStep.isRead; }
    uint8_t  getOpcode() const { return opcode; }
    bool     isStalledOut() const { return isStalled(); }
    uint64_t getCycle() const { return cycle; }

    void setTracer(Tracer* t) { tracer = t; }

    const char* currentOpName() const;
    const char* currentStepName() const;

    bool isAtInstructionBoundary() const { return nextOp == &CPU6502::decodeAndDispatch; }

    uint16_t PC = 0;
    uint8_t  S  = 0x00;
    uint8_t  A  = 0;
    uint8_t  X  = 0;
    uint8_t  Y  = 0;
    uint8_t  P  = FLAG_U | FLAG_B;

private:
    struct Step;
    using MicroOp = Step (CPU6502::*)();
    struct Step {
        MicroOp next;
        uint16_t busAddr;
        bool isRead;
        uint8_t writeData;
    };

    enum class IntKind : uint8_t { None, SoftwareBRK, IRQ, NMI };
    enum class ExecKind : uint8_t {
        LDA, LDX, LDY, AND_, ORA_, EOR_, BIT_, ADC_, SBC_, CMP_, CPX_, CPY_, NOP_,
        LAX_, ANC_, ALR_, ARR_, XAA_, AXS_, LAS_, LXA_,
        TAX, TAY, TSX, TXA, TXS, TYA, INX, INY, DEX, DEY, ASL_A, LSR_A, ROL_A, ROR_A,
        CLC, SEC, CLI, SEI, CLV, CLD, SED, NOP_IMP
    };
    enum class RmwKind : uint8_t {
        ASL, LSR, ROL, ROR, INC, DEC,
        SLO, RLA, SRE, RRA, DCP, ISC
    };
    enum class StoreKind : uint8_t {
        STA, STX, STY, SAX,
        SHX, SHY, SHA, SHS
    };
    enum class AccessMode : uint8_t { READ, RMW, WRITE };

    ICPUBus& bus;
    MicroOp nextOp = nullptr;
    MicroOp currentOp = nullptr;
    Step currentStep = { nullptr, 0, false, 0 };

    Tracer* tracer = nullptr;
    uint64_t cycle = 0;

    struct RegSnap { uint16_t PC; uint8_t A, X, Y, S, P; };

    uint8_t opcode = 0;
    uint16_t addr = 0;
    uint16_t ptr = 0;
    uint8_t fetched = 0;
    uint8_t tmp = 0;
    bool pageCross = false;
    bool nmiLevel = false;
    bool irqLevel = false;
    bool nmiPending = false;
    bool interruptPending = false;
    bool irqInhibitSnapshot = true;
    IntKind currentInt = IntKind::None;
    bool nmiEdgeDetected = false;
    bool nmiPollSignal = false;
    bool irqDetected = false;
    bool rdyLevel = false;
    bool skipNextPoll = false;
    ExecKind execKind = ExecKind::NOP_;
    RmwKind rmwKind = RmwKind::ASL;
    StoreKind storeKind = StoreKind::STA;
    AccessMode accessMode = AccessMode::READ;

    bool branchTaken = false;
    uint8_t branchOffset = 0;
    uint8_t origHigh = 0;

    static Step emitRead(MicroOp next, uint16_t busAddr) {
        return { next, busAddr, true, 0 };
    }

    static Step emitWrite(MicroOp next, uint16_t busAddr, uint8_t writeData) {
        return { next, busAddr, false, writeData };
    }

    uint16_t intVector() const { return currentInt == IntKind::NMI ? 0xFFFA : 0xFFFE; }

    bool isStalled() const { return !rdyLevel && currentStep.isRead; }

    void pollInterrupts() {
        if (nmiPollSignal) interruptPending = true;
        else if (irqDetected && !irqInhibitSnapshot) interruptPending = true;
    }

    void setZN(uint8_t v) {
        P = (P & ~(FLAG_Z | FLAG_N)) | (v == 0 ? FLAG_Z : 0) | (v & 0x80);
    }

    Step opFetch() {
        if (skipNextPoll) skipNextPoll = false;
        else pollInterrupts();
        if (interruptPending) {

            interruptPending = false;
            if (nmiPending) {
                nmiPending = false;
                currentInt = IntKind::NMI;
            } else {
                currentInt = IntKind::IRQ;
            }
            return emitRead(&CPU6502::int1_dummy, PC);
        }
        irqInhibitSnapshot = (P & FLAG_I) != 0;
        return emitRead(&CPU6502::decodeAndDispatch, PC++);
    }

    Step reset1() { return emitRead(&CPU6502::reset2, PC); }
    Step reset2() { return emitRead(&CPU6502::reset3, PC); }
    Step reset3() { Step s = emitRead(&CPU6502::reset4, 0x0100 | S); S--; return s; }
    Step reset4() { Step s = emitRead(&CPU6502::reset5, 0x0100 | S); S--; return s; }
    Step reset5() { Step s = emitRead(&CPU6502::reset6, 0x0100 | S); S--; return s; }
    Step reset6() { return emitRead(&CPU6502::reset7, 0xFFFC); }
    Step reset7() { addr = fetched; return emitRead(&CPU6502::reset8, 0xFFFD); }
    Step reset8() { addr |= fetched << 8; PC = addr; return opFetch(); }

    Step decodeAndDispatch();
    Step afterAddressing_ReadWrite();
    Step readAndExec_();
    Step executeWrite();
    Step rmw_dummyWrite();
    Step rmw_finalWrite();

    Step am_imm_exec();
    Step am_imp_exec();
    Step am_zp_1();
    Step am_zpx_1();  Step am_zpx_2();
    Step am_zpy_1();  Step am_zpy_2();
    Step am_abs_1();  Step am_abs_2();
    Step am_abx_1();  Step am_abx_2();  Step am_abx_3_fixup();
    Step am_aby_1();  Step am_aby_2();  Step am_aby_3_fixup();
    Step am_izx_1();  Step am_izx_2();  Step am_izx_3();  Step am_izx_4();
    Step am_izy_1();  Step am_izy_2();  Step am_izy_3();  Step am_izy_4_fixup();
    Step am_rel_1();  Step am_rel_2_taken();  Step am_rel_3_pagefix();
    Step am_ind_1();  Step am_ind_2();  Step am_ind_3();  Step am_ind_4();

    Step int1_dummy();
    Step int2_pushPCH();
    Step int3_pushPCL();
    Step int4_pushP();
    Step int5_readLow();
    Step int6_readHigh();

    Step rti1_dummyRead(); Step rti2_incS(); Step rti3_pullP();
    Step rti4_pullPCL();   Step rti5_pullPCH();

    Step rts1_dummyRead(); Step rts2_incS(); Step rts3_pullPCL();
    Step rts4_pullPCH();   Step rts5_incPC();

    Step jsr1_readLow();  Step jsr2_internal(); Step jsr3_pushPCH();
    Step jsr4_pushPCL();  Step jsr5_readHigh();

    Step pha1_dummy();
    Step php1_dummy();
    Step pla1_dummy(); Step pla2_incS(); Step pla3_pull();
    Step plp1_dummy(); Step plp2_incS(); Step plp3_pull();

    Step jmpAbs1_low(); Step jmpAbs2_high();
    Step jam_loop();

    void doExecADC() {
        uint16_t s = (uint16_t)A + fetched + ((P & FLAG_C) ? 1 : 0);
        auto r = (uint8_t)s;
        P = (P & ~(FLAG_C | FLAG_Z | FLAG_V | FLAG_N))
            | (s > 0xFF ? FLAG_C : 0) | (r == 0 ? FLAG_Z : 0)
            | (((~(A ^ fetched) & (A ^ r)) & 0x80) ? FLAG_V : 0) | (r & 0x80);
        A = r;
    }

    void doExecSBC() {
        uint8_t v = fetched ^ 0xFF;
        uint16_t s = (uint16_t)A + v + ((P & FLAG_C) ? 1 : 0);
        auto r = (uint8_t)s;
        P = (P & ~(FLAG_C | FLAG_Z | FLAG_V | FLAG_N))
            | (s > 0xFF ? FLAG_C : 0) | (r == 0 ? FLAG_Z : 0)
            | (((~(A ^ v) & (A ^ r)) & 0x80) ? FLAG_V : 0) | (r & 0x80);
        A = r;
    }

    void doExecRead() {
        switch (execKind) {
            case ExecKind::LDA: A = fetched; setZN(A); break;
            case ExecKind::LDX: X = fetched; setZN(X); break;
            case ExecKind::LDY: Y = fetched; setZN(Y); break;
            case ExecKind::AND_: A &= fetched; setZN(A); break;
            case ExecKind::ORA_: A |= fetched; setZN(A); break;
            case ExecKind::EOR_: A ^= fetched; setZN(A); break;
            case ExecKind::BIT_: {
                uint8_t r = A & fetched;
                P = (P & ~(FLAG_Z | FLAG_N | FLAG_V)) | (r == 0 ? FLAG_Z : 0) | (fetched & 0x80) | (fetched & 0x40);
            } break;
            case ExecKind::ADC_: doExecADC(); break;
            case ExecKind::SBC_: doExecSBC(); break;
            case ExecKind::CMP_: {
                uint16_t s = (uint16_t)A - fetched;
                P = (P & ~(FLAG_C|FLAG_Z|FLAG_N)) | (A >= fetched ? FLAG_C : 0) | (((uint8_t)s) == 0 ? FLAG_Z : 0) | (s & 0x80);
            } break;
            case ExecKind::CPX_: {
                uint16_t s = (uint16_t)X - fetched;
                P = (P & ~(FLAG_C|FLAG_Z|FLAG_N)) | (X >= fetched ? FLAG_C : 0) | (((uint8_t)s) == 0 ? FLAG_Z : 0) | (s & 0x80);
            } break;
            case ExecKind::CPY_: {
                uint16_t s = (uint16_t)Y - fetched;
                P = (P & ~(FLAG_C|FLAG_Z|FLAG_N)) | (Y >= fetched ? FLAG_C : 0) | (((uint8_t)s) == 0 ? FLAG_Z : 0) | (s & 0x80);
            } break;
            case ExecKind::NOP_: break;
            case ExecKind::LAX_: A = fetched; X = fetched; setZN(A); break;
            case ExecKind::ANC_: A &= fetched; setZN(A); P = (P & ~FLAG_C) | ((A & 0x80) ? FLAG_C : 0); break;
            case ExecKind::ALR_: A &= fetched; P = (P & ~FLAG_C) | ((A & 0x01) ? FLAG_C : 0); A >>= 1; setZN(A); break;
            case ExecKind::ARR_: {
                A &= fetched;
                uint8_t c = (P & FLAG_C) ? 0x80 : 0;
                A = (A >> 1) | c;
                setZN(A);
                P = (P & ~(FLAG_C | FLAG_V)) | ((A & 0x40) ? FLAG_C : 0) | (((A ^ (A << 1)) & 0x40) ? FLAG_V : 0);
            } break;
            case ExecKind::XAA_: A = (A | 0xEE) & X & fetched; setZN(A); break;
            case ExecKind::LXA_: A = (A | 0xFF) & fetched; X = A; setZN(A); break;
            case ExecKind::AXS_: {
                uint16_t s = (uint16_t)(A & X) - fetched;
                X = (uint8_t)s;
                P = (P & ~(FLAG_C | FLAG_Z | FLAG_N)) | (s < 0x100 ? FLAG_C : 0) | (X == 0 ? FLAG_Z : 0) | (X & 0x80);
            } break;
            case ExecKind::LAS_: {
                uint8_t v = fetched & S;
                A = v; X = v; S = v;
                setZN(v);
            } break;
            default: break;
        }
    }

    void doExecImplied() {
        switch (execKind) {
            case ExecKind::TAX: X = A; setZN(X); break;
            case ExecKind::TAY: Y = A; setZN(Y); break;
            case ExecKind::TSX: X = S; setZN(X); break;
            case ExecKind::TXA: A = X; setZN(A); break;
            case ExecKind::TXS: S = X; break;
            case ExecKind::TYA: A = Y; setZN(A); break;
            case ExecKind::INX: X++; setZN(X); break;
            case ExecKind::INY: Y++; setZN(Y); break;
            case ExecKind::DEX: X--; setZN(X); break;
            case ExecKind::DEY: Y--; setZN(Y); break;
            case ExecKind::ASL_A: P = (P & ~FLAG_C) | ((A & 0x80) ? FLAG_C : 0); A <<= 1; setZN(A); break;
            case ExecKind::LSR_A: P = (P & ~FLAG_C) | ((A & 0x01) ? FLAG_C : 0); A >>= 1; setZN(A); break;
            case ExecKind::ROL_A: { uint8_t c = (P & FLAG_C) ? 1 : 0; P = (P & ~FLAG_C) | ((A & 0x80) ? FLAG_C : 0); A = (A << 1) | c; setZN(A); } break;
            case ExecKind::ROR_A: { uint8_t c = (P & FLAG_C) ? 0x80 : 0; P = (P & ~FLAG_C) | ((A & 0x01) ? FLAG_C : 0); A = (A >> 1) | c; setZN(A); } break;
            case ExecKind::CLC: P &= ~FLAG_C; break;
            case ExecKind::SEC: P |=  FLAG_C; break;
            case ExecKind::CLI: P &= ~FLAG_I; break;
            case ExecKind::SEI: P |=  FLAG_I; break;
            case ExecKind::CLV: P &= ~FLAG_V; break;
            case ExecKind::CLD: P &= ~FLAG_D; break;
            case ExecKind::SED: P |=  FLAG_D; break;
            default: break;
        }
    }

    void doExecRmw() {
        switch (rmwKind) {
            case RmwKind::ASL: P = (P & ~FLAG_C) | ((fetched & 0x80) ? FLAG_C : 0); tmp = fetched << 1; setZN(tmp); break;
            case RmwKind::LSR: P = (P & ~FLAG_C) | ((fetched & 0x01) ? FLAG_C : 0); tmp = fetched >> 1; setZN(tmp); break;
            case RmwKind::ROL: { uint8_t c = (P & FLAG_C) ? 1 : 0; P = (P & ~FLAG_C) | ((fetched & 0x80) ? FLAG_C : 0); tmp = (fetched << 1) | c; setZN(tmp); } break;
            case RmwKind::ROR: { uint8_t c = (P & FLAG_C) ? 0x80 : 0; P = (P & ~FLAG_C) | ((fetched & 0x01) ? FLAG_C : 0); tmp = (fetched >> 1) | c; setZN(tmp); } break;
            case RmwKind::INC: tmp = fetched + 1; setZN(tmp); break;
            case RmwKind::DEC: tmp = fetched - 1; setZN(tmp); break;
            case RmwKind::SLO: P = (P & ~FLAG_C) | ((fetched & 0x80) ? FLAG_C : 0); tmp = fetched << 1; A |= tmp; setZN(A); break;
            case RmwKind::RLA: { uint8_t c = (P & FLAG_C) ? 1 : 0; P = (P & ~FLAG_C) | ((fetched & 0x80) ? FLAG_C : 0); tmp = (fetched << 1) | c; A &= tmp; setZN(A); } break;
            case RmwKind::SRE: P = (P & ~FLAG_C) | ((fetched & 0x01) ? FLAG_C : 0); tmp = fetched >> 1; A ^= tmp; setZN(A); break;
            case RmwKind::RRA: { uint8_t c = (P & FLAG_C) ? 0x80 : 0; P = (P & ~FLAG_C) | ((fetched & 0x01) ? FLAG_C : 0); tmp = (fetched >> 1) | c; fetched = tmp; doExecADC(); } break;
            case RmwKind::DCP: { tmp = fetched - 1; uint8_t r = A - tmp; P = (P & ~(FLAG_C | FLAG_Z | FLAG_N)) | (A >= tmp ? FLAG_C : 0) | (r == 0 ? FLAG_Z : 0) | (r & 0x80); } break;
            case RmwKind::ISC: { tmp = fetched + 1; fetched = tmp; doExecSBC(); } break;
        }
    }

    uint8_t storeValue() {
        switch (storeKind) {
            case StoreKind::STA: return A;
            case StoreKind::STX: return X;
            case StoreKind::STY: return Y;
            case StoreKind::SAX: return A & X;
            case StoreKind::SHX: return X & ((uint8_t)(origHigh + 1));
            case StoreKind::SHY: return Y & ((uint8_t)(origHigh + 1));
            case StoreKind::SHA: return A & X & ((uint8_t)(origHigh + 1));
            case StoreKind::SHS: S = A & X; return S & ((uint8_t)(origHigh + 1));
        }
        return 0;
    }

    static std::string flagStr(uint8_t p) {
        const char* names = "NV-BDIZC";
        std::string out(8, '-');
        for (int i = 0; i < 8; ++i) {
            bool set = (p >> (7 - i)) & 1;
            out[i] = set ? names[i] : (char)(names[i] | 0x20);
        }
        out[2] = (p & 0x20) ? 'U' : 'u';
        return out;
    }

    void emitTracePhi1(const RegSnap& pre, bool stalled) {
        if (!tracer || !tracer->cpu) return;
        const uint16_t a = currentStep.busAddr;
        const std::string sym = tracer->symbolExact(a);
        char addrField[48];
        if (currentStep.isRead) {
            std::snprintf(addrField, sizeof(addrField),
                          sym.empty() ? "R $%04X" : "R $%04X(%s)",
                          a, sym.c_str());
        } else {
            std::snprintf(addrField, sizeof(addrField),
                          sym.empty() ? "W $%04X=%02X" : "W $%04X=%02X(%s)",
                          a, currentStep.writeData, sym.c_str());
        }
        char body[256];
        const std::string near = tracer->symbolNear(pre.PC);
        std::snprintf(body, sizeof(body),
            "PHI1 PC=%04X A=%02X X=%02X Y=%02X S=%02X P=%s %-28s %-4s %-18s%s%s%s",
            pre.PC, pre.A, pre.X, pre.Y, pre.S, flagStr(pre.P).c_str(),
            stalled ? "STALL" : addrField,
            stalled ? "----" : currentOpName(),
            stalled ? "stalled" : currentStepName(),
            near.empty() ? "" : " ; ", near.c_str(), "");
        tracer->writeCpu(body);
    }

    void emitTracePhi2() {
        if (!tracer || !tracer->cpu) return;
        char body[64];
        if (currentStep.isRead && !bus.isReadOverridden())
            std::snprintf(body, sizeof(body), "PHI2 fetched=%02X", fetched);
        else if (!currentStep.isRead)
            std::snprintf(body, sizeof(body), "PHI2 wrote=%02X", currentStep.writeData);
        else
            std::snprintf(body, sizeof(body), "PHI2 (dma-override)");
        tracer->writeCpu(body);
    }
};

inline CPU6502::Step CPU6502::executeWrite() {
    uint8_t val = storeValue();
    bool isAnySh = (storeKind >= StoreKind::SHX);
    if (isAnySh && pageCross) {
        addr = ((uint16_t)val << 8) | (addr & 0xFF);
    }
    return emitWrite(&CPU6502::opFetch, addr, val);
}

inline CPU6502::Step CPU6502::afterAddressing_ReadWrite() {
    if (accessMode == AccessMode::READ)  return emitRead(&CPU6502::readAndExec_, addr);
    if (accessMode == AccessMode::RMW)   return emitRead(&CPU6502::rmw_dummyWrite, addr);
    return executeWrite();
}

inline CPU6502::Step CPU6502::readAndExec_() {
    doExecRead();
    return opFetch();
}

inline CPU6502::Step CPU6502::rmw_dummyWrite() {
    doExecRmw();
    return emitWrite(&CPU6502::rmw_finalWrite, addr, fetched);
}

inline CPU6502::Step CPU6502::rmw_finalWrite() {
    return emitWrite(&CPU6502::opFetch, addr, tmp);
}

inline CPU6502::Step CPU6502::am_imm_exec() {
    doExecRead();
    return opFetch();
}

inline CPU6502::Step CPU6502::am_imp_exec() {
    doExecImplied();
    return opFetch();
}

inline CPU6502::Step CPU6502::am_zp_1() {
    addr = fetched;
    return afterAddressing_ReadWrite();
}

inline CPU6502::Step CPU6502::am_zpx_1() { ptr = fetched; return emitRead(&CPU6502::am_zpx_2, ptr); }
inline CPU6502::Step CPU6502::am_zpx_2() { addr = (ptr + X) & 0xFF; return afterAddressing_ReadWrite(); }
inline CPU6502::Step CPU6502::am_zpy_1() { ptr = fetched; return emitRead(&CPU6502::am_zpy_2, ptr); }
inline CPU6502::Step CPU6502::am_zpy_2() { addr = (ptr + Y) & 0xFF; return afterAddressing_ReadWrite(); }

inline CPU6502::Step CPU6502::am_abs_1() { addr = fetched; return emitRead(&CPU6502::am_abs_2, PC++); }
inline CPU6502::Step CPU6502::am_abs_2() { addr |= fetched << 8; return afterAddressing_ReadWrite(); }

inline CPU6502::Step CPU6502::am_abx_1() { addr = fetched; return emitRead(&CPU6502::am_abx_2, PC++); }
inline CPU6502::Step CPU6502::am_abx_2() {
    origHigh = fetched;
    uint16_t base = (origHigh << 8) | (addr & 0xFF);
    uint16_t eff  = base + X;
    pageCross = (eff & 0xFF00) != (base & 0xFF00);
    addr = eff;
    if (accessMode != AccessMode::READ || pageCross) return emitRead(&CPU6502::am_abx_3_fixup, addr - (pageCross ? 0x100 : 0));
    return afterAddressing_ReadWrite();
}
inline CPU6502::Step CPU6502::am_abx_3_fixup() { return afterAddressing_ReadWrite(); }

inline CPU6502::Step CPU6502::am_aby_1() { addr = fetched; return emitRead(&CPU6502::am_aby_2, PC++); }
inline CPU6502::Step CPU6502::am_aby_2() {
    origHigh = fetched;
    uint16_t base = (origHigh << 8) | (addr & 0xFF);
    uint16_t eff  = base + Y;
    pageCross = (eff & 0xFF00) != (base & 0xFF00);
    addr = eff;
    if (accessMode != AccessMode::READ || pageCross) return emitRead(&CPU6502::am_aby_3_fixup, addr - (pageCross ? 0x100 : 0));
    return afterAddressing_ReadWrite();
}
inline CPU6502::Step CPU6502::am_aby_3_fixup() { return afterAddressing_ReadWrite(); }

inline CPU6502::Step CPU6502::am_izx_1() { ptr = fetched; return emitRead(&CPU6502::am_izx_2, ptr); }
inline CPU6502::Step CPU6502::am_izx_2() { ptr = (ptr + X) & 0xFF; return emitRead(&CPU6502::am_izx_3, ptr); }
inline CPU6502::Step CPU6502::am_izx_3() { addr = fetched; return emitRead(&CPU6502::am_izx_4, (ptr + 1) & 0xFF); }
inline CPU6502::Step CPU6502::am_izx_4() { addr |= fetched << 8; return afterAddressing_ReadWrite(); }

inline CPU6502::Step CPU6502::am_izy_1() { ptr = fetched; return emitRead(&CPU6502::am_izy_2, ptr); }
inline CPU6502::Step CPU6502::am_izy_2() { addr = fetched; return emitRead(&CPU6502::am_izy_3, (ptr + 1) & 0xFF); }
inline CPU6502::Step CPU6502::am_izy_3() {
    origHigh = fetched;
    uint16_t base = (origHigh << 8) | (addr & 0xFF);
    uint16_t eff  = base + Y;
    pageCross = (eff & 0xFF00) != (base & 0xFF00);
    addr = eff;
    if (accessMode != AccessMode::READ || pageCross) return emitRead(&CPU6502::am_izy_4_fixup, addr - (pageCross ? 0x100 : 0));
    return afterAddressing_ReadWrite();
}
inline CPU6502::Step CPU6502::am_izy_4_fixup() { return afterAddressing_ReadWrite(); }

inline CPU6502::Step CPU6502::am_rel_1() {
    branchOffset = fetched;
    if (!branchTaken) return opFetch();
    uint8_t oldL = PC & 0xFF;
    uint8_t newL = oldL + branchOffset;
    bool wouldCross = ((int8_t)branchOffset < 0) ? (newL > oldL) : (newL < oldL);
    if (wouldCross) pollInterrupts();
    return emitRead(&CPU6502::am_rel_2_taken, PC);
}
inline CPU6502::Step CPU6502::am_rel_2_taken() {
    uint16_t oldPC = PC;
    uint8_t  oldL  = PC & 0xFF;
    uint8_t  newL  = oldL + branchOffset;
    PC = (oldPC & 0xFF00) | newL;
    bool cross = ((int8_t)branchOffset < 0) ? (newL > oldL) : (newL < oldL);
    if (!cross) { skipNextPoll = true; return opFetch(); }

    pageCross = ((int8_t)branchOffset < 0);
    return emitRead(&CPU6502::am_rel_3_pagefix, PC);
}
inline CPU6502::Step CPU6502::am_rel_3_pagefix() {
    PC += pageCross ? (uint16_t)(-0x100) : 0x100;
    return opFetch();
}

inline CPU6502::Step CPU6502::am_ind_1() { ptr = fetched; return emitRead(&CPU6502::am_ind_2, PC++); }
inline CPU6502::Step CPU6502::am_ind_2() { ptr |= fetched << 8; return emitRead(&CPU6502::am_ind_3, ptr); }
inline CPU6502::Step CPU6502::am_ind_3() { addr = fetched; uint16_t hiAddr = (ptr & 0xFF00) | ((ptr + 1) & 0xFF); return emitRead(&CPU6502::am_ind_4, hiAddr); }
inline CPU6502::Step CPU6502::am_ind_4() { addr |= fetched << 8; PC = addr; return opFetch(); }

inline CPU6502::Step CPU6502::jmpAbs1_low()  { addr = fetched; return emitRead(&CPU6502::jmpAbs2_high, PC++); }
inline CPU6502::Step CPU6502::jmpAbs2_high() { addr |= fetched << 8; PC = addr; return opFetch(); }

inline CPU6502::Step CPU6502::int1_dummy() {
    if (currentInt == IntKind::SoftwareBRK) PC++;
    return emitWrite(&CPU6502::int2_pushPCH, 0x0100 | S--, (PC >> 8) & 0xFF);
}
inline CPU6502::Step CPU6502::int2_pushPCH() { return emitWrite(&CPU6502::int3_pushPCL, 0x0100 | S--, PC & 0xFF); }
inline CPU6502::Step CPU6502::int3_pushPCL() {
    bool wasBRK = (currentInt == IntKind::SoftwareBRK);
    if (currentInt != IntKind::NMI && nmiEdgeDetected) {
        nmiPending = false;
        currentInt = IntKind::NMI;
    } else if (currentInt == IntKind::SoftwareBRK && irqDetected && !irqInhibitSnapshot) {
        currentInt = IntKind::IRQ;
    }
    uint8_t pushP = P | FLAG_U | (wasBRK ? FLAG_B : 0);
    P |= FLAG_I;
    irqInhibitSnapshot = true;
    return emitWrite(&CPU6502::int5_readLow, 0x0100 | S--, pushP);
}
inline CPU6502::Step CPU6502::int5_readLow()  { return emitRead(&CPU6502::int6_readHigh, intVector()); }
inline CPU6502::Step CPU6502::int6_readHigh() { addr = fetched; return emitRead(&CPU6502::rti5_pullPCH, intVector() + 1); }

inline CPU6502::Step CPU6502::rti1_dummyRead() { return emitRead(&CPU6502::rti2_incS, 0x0100 | S); }
inline CPU6502::Step CPU6502::rti2_incS()      { S++; return emitRead(&CPU6502::rti3_pullP, 0x0100 | S); }
inline CPU6502::Step CPU6502::rti3_pullP()     { P = (fetched & ~FLAG_B) | FLAG_U; irqInhibitSnapshot = (P & FLAG_I) != 0; S++; return emitRead(&CPU6502::rti4_pullPCL, 0x0100 | S); }
inline CPU6502::Step CPU6502::rti4_pullPCL()   { addr = fetched; S++; return emitRead(&CPU6502::rti5_pullPCH, 0x0100 | S); }
inline CPU6502::Step CPU6502::rti5_pullPCH()   { addr |= fetched << 8; PC = addr; return opFetch(); }

inline CPU6502::Step CPU6502::rts1_dummyRead() { return emitRead(&CPU6502::rts2_incS, 0x0100 | S); }
inline CPU6502::Step CPU6502::rts2_incS()      { S++; return emitRead(&CPU6502::rts3_pullPCL, 0x0100 | S); }
inline CPU6502::Step CPU6502::rts3_pullPCL()   { addr = fetched; S++; return emitRead(&CPU6502::rts4_pullPCH, 0x0100 | S); }
inline CPU6502::Step CPU6502::rts4_pullPCH()   { addr |= fetched << 8; PC = addr; return emitRead(&CPU6502::rts5_incPC, PC); }
inline CPU6502::Step CPU6502::rts5_incPC()     { PC++; return opFetch(); }

inline CPU6502::Step CPU6502::jsr1_readLow()  { addr = fetched; return emitRead(&CPU6502::jsr2_internal, 0x0100 | S); }
inline CPU6502::Step CPU6502::jsr2_internal() { return emitWrite(&CPU6502::jsr3_pushPCH, 0x0100 | S--, (PC >> 8) & 0xFF); }
inline CPU6502::Step CPU6502::jsr3_pushPCH()  { return emitWrite(&CPU6502::jsr4_pushPCL, 0x0100 | S--, PC & 0xFF); }
inline CPU6502::Step CPU6502::jsr4_pushPCL()  { return emitRead(&CPU6502::jsr5_readHigh, PC); }
inline CPU6502::Step CPU6502::jsr5_readHigh() { addr |= fetched << 8; PC = addr; return opFetch(); }

inline CPU6502::Step CPU6502::pha1_dummy() { return emitWrite(&CPU6502::opFetch, 0x0100 | S--, A); }
inline CPU6502::Step CPU6502::php1_dummy() { return emitWrite(&CPU6502::opFetch, 0x0100 | S--, P | FLAG_B | FLAG_U); }
inline CPU6502::Step CPU6502::pla1_dummy() { return emitRead(&CPU6502::pla2_incS, 0x0100 | S); }
inline CPU6502::Step CPU6502::pla2_incS()  { S++; return emitRead(&CPU6502::pla3_pull, 0x0100 | S); }
inline CPU6502::Step CPU6502::pla3_pull()  { A = fetched; setZN(A); return opFetch(); }
inline CPU6502::Step CPU6502::plp1_dummy() { return emitRead(&CPU6502::plp2_incS, 0x0100 | S); }
inline CPU6502::Step CPU6502::plp2_incS()  { S++; return emitRead(&CPU6502::plp3_pull, 0x0100 | S); }
inline CPU6502::Step CPU6502::plp3_pull()  { P = (fetched & ~FLAG_B) | FLAG_U; return opFetch(); }

inline CPU6502::Step CPU6502::jam_loop() {
    return emitRead(&CPU6502::jam_loop, PC);
}

inline CPU6502::Step CPU6502::decodeAndDispatch() {
    branchTaken = false;
    accessMode  = AccessMode::READ;
    execKind    = ExecKind::NOP_;
    opcode = fetched;

    auto setRead  = [&](ExecKind k, MicroOp am1) { execKind = k; accessMode = AccessMode::READ;  return emitRead(am1, PC++); };
    auto setWrite = [&](StoreKind k, MicroOp am1){ storeKind = k; accessMode = AccessMode::WRITE; return emitRead(am1, PC++); };
    auto setRmw   = [&](RmwKind k, MicroOp am1)  { rmwKind = k;  accessMode = AccessMode::RMW;   return emitRead(am1, PC++); };
    auto branch   = [&](bool cond)               { branchTaken = cond; return emitRead(&CPU6502::am_rel_1, PC++); };
    auto implied  = [&](ExecKind k)              { execKind = k; return emitRead(&CPU6502::am_imp_exec, PC); };

    switch (opcode) {
    case 0xA9: return setRead(ExecKind::LDA, &CPU6502::am_imm_exec);
    case 0xA5: return setRead(ExecKind::LDA, &CPU6502::am_zp_1);
    case 0xB5: return setRead(ExecKind::LDA, &CPU6502::am_zpx_1);
    case 0xAD: return setRead(ExecKind::LDA, &CPU6502::am_abs_1);
    case 0xBD: return setRead(ExecKind::LDA, &CPU6502::am_abx_1);
    case 0xB9: return setRead(ExecKind::LDA, &CPU6502::am_aby_1);
    case 0xA1: return setRead(ExecKind::LDA, &CPU6502::am_izx_1);
    case 0xB1: return setRead(ExecKind::LDA, &CPU6502::am_izy_1);

    case 0xA2: return setRead(ExecKind::LDX, &CPU6502::am_imm_exec);
    case 0xA6: return setRead(ExecKind::LDX, &CPU6502::am_zp_1);
    case 0xB6: return setRead(ExecKind::LDX, &CPU6502::am_zpy_1);
    case 0xAE: return setRead(ExecKind::LDX, &CPU6502::am_abs_1);
    case 0xBE: return setRead(ExecKind::LDX, &CPU6502::am_aby_1);

    case 0xA0: return setRead(ExecKind::LDY, &CPU6502::am_imm_exec);
    case 0xA4: return setRead(ExecKind::LDY, &CPU6502::am_zp_1);
    case 0xB4: return setRead(ExecKind::LDY, &CPU6502::am_zpx_1);
    case 0xAC: return setRead(ExecKind::LDY, &CPU6502::am_abs_1);
    case 0xBC: return setRead(ExecKind::LDY, &CPU6502::am_abx_1);

    case 0x85: return setWrite(StoreKind::STA, &CPU6502::am_zp_1);
    case 0x95: return setWrite(StoreKind::STA, &CPU6502::am_zpx_1);
    case 0x8D: return setWrite(StoreKind::STA, &CPU6502::am_abs_1);
    case 0x9D: return setWrite(StoreKind::STA, &CPU6502::am_abx_1);
    case 0x99: return setWrite(StoreKind::STA, &CPU6502::am_aby_1);
    case 0x81: return setWrite(StoreKind::STA, &CPU6502::am_izx_1);
    case 0x91: return setWrite(StoreKind::STA, &CPU6502::am_izy_1);

    case 0x86: return setWrite(StoreKind::STX, &CPU6502::am_zp_1);
    case 0x96: return setWrite(StoreKind::STX, &CPU6502::am_zpy_1);
    case 0x8E: return setWrite(StoreKind::STX, &CPU6502::am_abs_1);

    case 0x84: return setWrite(StoreKind::STY, &CPU6502::am_zp_1);
    case 0x94: return setWrite(StoreKind::STY, &CPU6502::am_zpx_1);
    case 0x8C: return setWrite(StoreKind::STY, &CPU6502::am_abs_1);

    case 0xAA: return implied(ExecKind::TAX);
    case 0xA8: return implied(ExecKind::TAY);
    case 0xBA: return implied(ExecKind::TSX);
    case 0x8A: return implied(ExecKind::TXA);
    case 0x9A: return implied(ExecKind::TXS);
    case 0x98: return implied(ExecKind::TYA);

    case 0x48: return emitRead(&CPU6502::pha1_dummy, PC);
    case 0x08: return emitRead(&CPU6502::php1_dummy, PC);
    case 0x68: return emitRead(&CPU6502::pla1_dummy, PC);
    case 0x28: return emitRead(&CPU6502::plp1_dummy, PC);

    case 0x29: return setRead(ExecKind::AND_, &CPU6502::am_imm_exec);
    case 0x25: return setRead(ExecKind::AND_, &CPU6502::am_zp_1);
    case 0x35: return setRead(ExecKind::AND_, &CPU6502::am_zpx_1);
    case 0x2D: return setRead(ExecKind::AND_, &CPU6502::am_abs_1);
    case 0x3D: return setRead(ExecKind::AND_, &CPU6502::am_abx_1);
    case 0x39: return setRead(ExecKind::AND_, &CPU6502::am_aby_1);
    case 0x21: return setRead(ExecKind::AND_, &CPU6502::am_izx_1);
    case 0x31: return setRead(ExecKind::AND_, &CPU6502::am_izy_1);

    case 0x09: return setRead(ExecKind::ORA_, &CPU6502::am_imm_exec);
    case 0x05: return setRead(ExecKind::ORA_, &CPU6502::am_zp_1);
    case 0x15: return setRead(ExecKind::ORA_, &CPU6502::am_zpx_1);
    case 0x0D: return setRead(ExecKind::ORA_, &CPU6502::am_abs_1);
    case 0x1D: return setRead(ExecKind::ORA_, &CPU6502::am_abx_1);
    case 0x19: return setRead(ExecKind::ORA_, &CPU6502::am_aby_1);
    case 0x01: return setRead(ExecKind::ORA_, &CPU6502::am_izx_1);
    case 0x11: return setRead(ExecKind::ORA_, &CPU6502::am_izy_1);

    case 0x49: return setRead(ExecKind::EOR_, &CPU6502::am_imm_exec);
    case 0x45: return setRead(ExecKind::EOR_, &CPU6502::am_zp_1);
    case 0x55: return setRead(ExecKind::EOR_, &CPU6502::am_zpx_1);
    case 0x4D: return setRead(ExecKind::EOR_, &CPU6502::am_abs_1);
    case 0x5D: return setRead(ExecKind::EOR_, &CPU6502::am_abx_1);
    case 0x59: return setRead(ExecKind::EOR_, &CPU6502::am_aby_1);
    case 0x41: return setRead(ExecKind::EOR_, &CPU6502::am_izx_1);
    case 0x51: return setRead(ExecKind::EOR_, &CPU6502::am_izy_1);

    case 0x24: return setRead(ExecKind::BIT_, &CPU6502::am_zp_1);
    case 0x2C: return setRead(ExecKind::BIT_, &CPU6502::am_abs_1);

    case 0x69: return setRead(ExecKind::ADC_, &CPU6502::am_imm_exec);
    case 0x65: return setRead(ExecKind::ADC_, &CPU6502::am_zp_1);
    case 0x75: return setRead(ExecKind::ADC_, &CPU6502::am_zpx_1);
    case 0x6D: return setRead(ExecKind::ADC_, &CPU6502::am_abs_1);
    case 0x7D: return setRead(ExecKind::ADC_, &CPU6502::am_abx_1);
    case 0x79: return setRead(ExecKind::ADC_, &CPU6502::am_aby_1);
    case 0x61: return setRead(ExecKind::ADC_, &CPU6502::am_izx_1);
    case 0x71: return setRead(ExecKind::ADC_, &CPU6502::am_izy_1);

    case 0xE9: case 0xEB: return setRead(ExecKind::SBC_, &CPU6502::am_imm_exec);
    case 0xE5: return setRead(ExecKind::SBC_, &CPU6502::am_zp_1);
    case 0xF5: return setRead(ExecKind::SBC_, &CPU6502::am_zpx_1);
    case 0xED: return setRead(ExecKind::SBC_, &CPU6502::am_abs_1);
    case 0xFD: return setRead(ExecKind::SBC_, &CPU6502::am_abx_1);
    case 0xF9: return setRead(ExecKind::SBC_, &CPU6502::am_aby_1);
    case 0xE1: return setRead(ExecKind::SBC_, &CPU6502::am_izx_1);
    case 0xF1: return setRead(ExecKind::SBC_, &CPU6502::am_izy_1);

    case 0xC9: return setRead(ExecKind::CMP_, &CPU6502::am_imm_exec);
    case 0xC5: return setRead(ExecKind::CMP_, &CPU6502::am_zp_1);
    case 0xD5: return setRead(ExecKind::CMP_, &CPU6502::am_zpx_1);
    case 0xCD: return setRead(ExecKind::CMP_, &CPU6502::am_abs_1);
    case 0xDD: return setRead(ExecKind::CMP_, &CPU6502::am_abx_1);
    case 0xD9: return setRead(ExecKind::CMP_, &CPU6502::am_aby_1);
    case 0xC1: return setRead(ExecKind::CMP_, &CPU6502::am_izx_1);
    case 0xD1: return setRead(ExecKind::CMP_, &CPU6502::am_izy_1);

    case 0xE0: return setRead(ExecKind::CPX_, &CPU6502::am_imm_exec);
    case 0xE4: return setRead(ExecKind::CPX_, &CPU6502::am_zp_1);
    case 0xEC: return setRead(ExecKind::CPX_, &CPU6502::am_abs_1);

    case 0xC0: return setRead(ExecKind::CPY_, &CPU6502::am_imm_exec);
    case 0xC4: return setRead(ExecKind::CPY_, &CPU6502::am_zp_1);
    case 0xCC: return setRead(ExecKind::CPY_, &CPU6502::am_abs_1);

    case 0xE8: return implied(ExecKind::INX);
    case 0xC8: return implied(ExecKind::INY);
    case 0xCA: return implied(ExecKind::DEX);
    case 0x88: return implied(ExecKind::DEY);

    case 0xE6: return setRmw(RmwKind::INC, &CPU6502::am_zp_1);
    case 0xF6: return setRmw(RmwKind::INC, &CPU6502::am_zpx_1);
    case 0xEE: return setRmw(RmwKind::INC, &CPU6502::am_abs_1);
    case 0xFE: return setRmw(RmwKind::INC, &CPU6502::am_abx_1);
    case 0xC6: return setRmw(RmwKind::DEC, &CPU6502::am_zp_1);
    case 0xD6: return setRmw(RmwKind::DEC, &CPU6502::am_zpx_1);
    case 0xCE: return setRmw(RmwKind::DEC, &CPU6502::am_abs_1);
    case 0xDE: return setRmw(RmwKind::DEC, &CPU6502::am_abx_1);

    case 0x0A: return implied(ExecKind::ASL_A);
    case 0x4A: return implied(ExecKind::LSR_A);
    case 0x2A: return implied(ExecKind::ROL_A);
    case 0x6A: return implied(ExecKind::ROR_A);

    case 0x06: return setRmw(RmwKind::ASL, &CPU6502::am_zp_1);
    case 0x16: return setRmw(RmwKind::ASL, &CPU6502::am_zpx_1);
    case 0x0E: return setRmw(RmwKind::ASL, &CPU6502::am_abs_1);
    case 0x1E: return setRmw(RmwKind::ASL, &CPU6502::am_abx_1);
    case 0x46: return setRmw(RmwKind::LSR, &CPU6502::am_zp_1);
    case 0x56: return setRmw(RmwKind::LSR, &CPU6502::am_zpx_1);
    case 0x4E: return setRmw(RmwKind::LSR, &CPU6502::am_abs_1);
    case 0x5E: return setRmw(RmwKind::LSR, &CPU6502::am_abx_1);
    case 0x26: return setRmw(RmwKind::ROL, &CPU6502::am_zp_1);
    case 0x36: return setRmw(RmwKind::ROL, &CPU6502::am_zpx_1);
    case 0x2E: return setRmw(RmwKind::ROL, &CPU6502::am_abs_1);
    case 0x3E: return setRmw(RmwKind::ROL, &CPU6502::am_abx_1);
    case 0x66: return setRmw(RmwKind::ROR, &CPU6502::am_zp_1);
    case 0x76: return setRmw(RmwKind::ROR, &CPU6502::am_zpx_1);
    case 0x6E: return setRmw(RmwKind::ROR, &CPU6502::am_abs_1);
    case 0x7E: return setRmw(RmwKind::ROR, &CPU6502::am_abx_1);

    case 0x18: return implied(ExecKind::CLC);
    case 0x38: return implied(ExecKind::SEC);
    case 0x58: return implied(ExecKind::CLI);
    case 0x78: return implied(ExecKind::SEI);
    case 0xB8: return implied(ExecKind::CLV);
    case 0xD8: return implied(ExecKind::CLD);
    case 0xF8: return implied(ExecKind::SED);

    case 0x10: return branch(!(P & FLAG_N));
    case 0x30: return branch( (P & FLAG_N));
    case 0x50: return branch(!(P & FLAG_V));
    case 0x70: return branch( (P & FLAG_V));
    case 0x90: return branch(!(P & FLAG_C));
    case 0xB0: return branch( (P & FLAG_C));
    case 0xD0: return branch(!(P & FLAG_Z));
    case 0xF0: return branch( (P & FLAG_Z));

    case 0x4C: return emitRead(&CPU6502::jmpAbs1_low, PC++);
    case 0x6C: return emitRead(&CPU6502::am_ind_1, PC++);
    case 0x20: return emitRead(&CPU6502::jsr1_readLow, PC++);
    case 0x60: return emitRead(&CPU6502::rts1_dummyRead, PC);
    case 0x40: return emitRead(&CPU6502::rti1_dummyRead, PC);

    case 0x00: currentInt = IntKind::SoftwareBRK; return emitRead(&CPU6502::int1_dummy, PC);

    case 0x1A: case 0x3A: case 0x5A: case 0x7A: case 0xDA: case 0xEA: case 0xFA: return implied(ExecKind::NOP_IMP);

    case 0x80: case 0x82: case 0x89: case 0xC2: case 0xE2: return setRead(ExecKind::NOP_, &CPU6502::am_imm_exec);
    case 0x04: case 0x44: case 0x64: return setRead(ExecKind::NOP_, &CPU6502::am_zp_1);
    case 0x14: case 0x34: case 0x54: case 0x74: case 0xD4: case 0xF4: return setRead(ExecKind::NOP_, &CPU6502::am_zpx_1);
    case 0x0C: return setRead(ExecKind::NOP_, &CPU6502::am_abs_1);
    case 0x1C: case 0x3C: case 0x5C: case 0x7C: case 0xDC: case 0xFC: return setRead(ExecKind::NOP_, &CPU6502::am_abx_1);

    case 0xA7: return setRead(ExecKind::LAX_, &CPU6502::am_zp_1);
    case 0xB7: return setRead(ExecKind::LAX_, &CPU6502::am_zpy_1);
    case 0xAF: return setRead(ExecKind::LAX_, &CPU6502::am_abs_1);
    case 0xBF: return setRead(ExecKind::LAX_, &CPU6502::am_aby_1);
    case 0xA3: return setRead(ExecKind::LAX_, &CPU6502::am_izx_1);
    case 0xB3: return setRead(ExecKind::LAX_, &CPU6502::am_izy_1);
    case 0xAB: return setRead(ExecKind::LXA_, &CPU6502::am_imm_exec);

    case 0x87: return setWrite(StoreKind::SAX, &CPU6502::am_zp_1);
    case 0x97: return setWrite(StoreKind::SAX, &CPU6502::am_zpy_1);
    case 0x8F: return setWrite(StoreKind::SAX, &CPU6502::am_abs_1);
    case 0x83: return setWrite(StoreKind::SAX, &CPU6502::am_izx_1);

    case 0x0B: case 0x2B: return setRead(ExecKind::ANC_, &CPU6502::am_imm_exec);
    case 0x4B:            return setRead(ExecKind::ALR_, &CPU6502::am_imm_exec);
    case 0x6B:            return setRead(ExecKind::ARR_, &CPU6502::am_imm_exec);
    case 0x8B:            return setRead(ExecKind::XAA_, &CPU6502::am_imm_exec);
    case 0xCB:            return setRead(ExecKind::AXS_, &CPU6502::am_imm_exec);

    case 0xBB: return setRead(ExecKind::LAS_, &CPU6502::am_aby_1);

    case 0x07: return setRmw(RmwKind::SLO, &CPU6502::am_zp_1);
    case 0x17: return setRmw(RmwKind::SLO, &CPU6502::am_zpx_1);
    case 0x0F: return setRmw(RmwKind::SLO, &CPU6502::am_abs_1);
    case 0x1F: return setRmw(RmwKind::SLO, &CPU6502::am_abx_1);
    case 0x1B: return setRmw(RmwKind::SLO, &CPU6502::am_aby_1);
    case 0x03: return setRmw(RmwKind::SLO, &CPU6502::am_izx_1);
    case 0x13: return setRmw(RmwKind::SLO, &CPU6502::am_izy_1);

    case 0x27: return setRmw(RmwKind::RLA, &CPU6502::am_zp_1);
    case 0x37: return setRmw(RmwKind::RLA, &CPU6502::am_zpx_1);
    case 0x2F: return setRmw(RmwKind::RLA, &CPU6502::am_abs_1);
    case 0x3F: return setRmw(RmwKind::RLA, &CPU6502::am_abx_1);
    case 0x3B: return setRmw(RmwKind::RLA, &CPU6502::am_aby_1);
    case 0x23: return setRmw(RmwKind::RLA, &CPU6502::am_izx_1);
    case 0x33: return setRmw(RmwKind::RLA, &CPU6502::am_izy_1);

    case 0x47: return setRmw(RmwKind::SRE, &CPU6502::am_zp_1);
    case 0x57: return setRmw(RmwKind::SRE, &CPU6502::am_zpx_1);
    case 0x4F: return setRmw(RmwKind::SRE, &CPU6502::am_abs_1);
    case 0x5F: return setRmw(RmwKind::SRE, &CPU6502::am_abx_1);
    case 0x5B: return setRmw(RmwKind::SRE, &CPU6502::am_aby_1);
    case 0x43: return setRmw(RmwKind::SRE, &CPU6502::am_izx_1);
    case 0x53: return setRmw(RmwKind::SRE, &CPU6502::am_izy_1);

    case 0x67: return setRmw(RmwKind::RRA, &CPU6502::am_zp_1);
    case 0x77: return setRmw(RmwKind::RRA, &CPU6502::am_zpx_1);
    case 0x6F: return setRmw(RmwKind::RRA, &CPU6502::am_abs_1);
    case 0x7F: return setRmw(RmwKind::RRA, &CPU6502::am_abx_1);
    case 0x7B: return setRmw(RmwKind::RRA, &CPU6502::am_aby_1);
    case 0x63: return setRmw(RmwKind::RRA, &CPU6502::am_izx_1);
    case 0x73: return setRmw(RmwKind::RRA, &CPU6502::am_izy_1);

    case 0xC7: return setRmw(RmwKind::DCP, &CPU6502::am_zp_1);
    case 0xD7: return setRmw(RmwKind::DCP, &CPU6502::am_zpx_1);
    case 0xCF: return setRmw(RmwKind::DCP, &CPU6502::am_abs_1);
    case 0xDF: return setRmw(RmwKind::DCP, &CPU6502::am_abx_1);
    case 0xDB: return setRmw(RmwKind::DCP, &CPU6502::am_aby_1);
    case 0xC3: return setRmw(RmwKind::DCP, &CPU6502::am_izx_1);
    case 0xD3: return setRmw(RmwKind::DCP, &CPU6502::am_izy_1);

    case 0xE7: return setRmw(RmwKind::ISC, &CPU6502::am_zp_1);
    case 0xF7: return setRmw(RmwKind::ISC, &CPU6502::am_zpx_1);
    case 0xEF: return setRmw(RmwKind::ISC, &CPU6502::am_abs_1);
    case 0xFF: return setRmw(RmwKind::ISC, &CPU6502::am_abx_1);
    case 0xFB: return setRmw(RmwKind::ISC, &CPU6502::am_aby_1);
    case 0xE3: return setRmw(RmwKind::ISC, &CPU6502::am_izx_1);
    case 0xF3: return setRmw(RmwKind::ISC, &CPU6502::am_izy_1);

    case 0x9C: return setWrite(StoreKind::SHY, &CPU6502::am_abx_1);
    case 0x9E: return setWrite(StoreKind::SHX, &CPU6502::am_aby_1);
    case 0x9F: return setWrite(StoreKind::SHA, &CPU6502::am_aby_1);
    case 0x93: return setWrite(StoreKind::SHA, &CPU6502::am_izy_1);
    case 0x9B: return setWrite(StoreKind::SHS, &CPU6502::am_aby_1);

    case 0x02: case 0x12: case 0x22: case 0x32: case 0x42: case 0x52:
    case 0x62: case 0x72: case 0x92: case 0xB2: case 0xD2: case 0xF2:
        return emitRead(&CPU6502::jam_loop, PC);

    default: return implied(ExecKind::NOP_IMP);
    }
}

inline const char* CPU6502::currentOpName() const {
    static const char* names[256] = {
        "BRK","ORA","JAM","SLO","NOP","ORA","ASL","SLO","PHP","ORA","ASL","ANC","NOP","ORA","ASL","SLO",
        "BPL","ORA","JAM","SLO","NOP","ORA","ASL","SLO","CLC","ORA","NOP","SLO","NOP","ORA","ASL","SLO",
        "JSR","AND","JAM","RLA","BIT","AND","ROL","RLA","PLP","AND","ROL","ANC","BIT","AND","ROL","RLA",
        "BMI","AND","JAM","RLA","NOP","AND","ROL","RLA","SEC","AND","NOP","RLA","NOP","AND","ROL","RLA",
        "RTI","EOR","JAM","SRE","NOP","EOR","LSR","SRE","PHA","EOR","LSR","ALR","JMP","EOR","LSR","SRE",
        "BVC","EOR","JAM","SRE","NOP","EOR","LSR","SRE","CLI","EOR","NOP","SRE","NOP","EOR","LSR","SRE",
        "RTS","ADC","JAM","RRA","NOP","ADC","ROR","RRA","PLA","ADC","ROR","ARR","JMP","ADC","ROR","RRA",
        "BVS","ADC","JAM","RRA","NOP","ADC","ROR","RRA","SEI","ADC","NOP","RRA","NOP","ADC","ROR","RRA",
        "NOP","STA","NOP","SAX","STY","STA","STX","SAX","DEY","NOP","TXA","XAA","STY","STA","STX","SAX",
        "BCC","STA","JAM","SHA","STY","STA","STX","SAX","TYA","STA","TXS","SHS","SHY","STA","SHX","SHA",
        "LDY","LDA","LDX","LAX","LDY","LDA","LDX","LAX","TAY","LDA","TAX","LXA","LDY","LDA","LDX","LAX",
        "BCS","LDA","JAM","LAX","LDY","LDA","LDX","LAX","CLV","LDA","TSX","LAS","LDY","LDA","LDX","LAX",
        "CPY","CMP","NOP","DCP","CPY","CMP","DEC","DCP","INY","CMP","DEX","AXS","CPY","CMP","DEC","DCP",
        "BNE","CMP","JAM","DCP","NOP","CMP","DEC","DCP","CLD","CMP","NOP","DCP","NOP","CMP","DEC","DCP",
        "CPX","SBC","NOP","ISC","CPX","SBC","INC","ISC","INX","SBC","NOP","SBC","CPX","SBC","INC","ISC",
        "BEQ","SBC","JAM","ISC","NOP","SBC","INC","ISC","SED","SBC","NOP","ISC","NOP","SBC","INC","ISC",
    };
    return names[opcode];
}

inline const char* CPU6502::currentStepName() const {
    #define CPU_STEP(fn) if (currentOp == &CPU6502::fn) return #fn
    CPU_STEP(opFetch);
    CPU_STEP(decodeAndDispatch);
    CPU_STEP(reset1);  CPU_STEP(reset2);  CPU_STEP(reset3);  CPU_STEP(reset4);
    CPU_STEP(reset5);  CPU_STEP(reset6);  CPU_STEP(reset7);  CPU_STEP(reset8);
    CPU_STEP(am_imm_exec); CPU_STEP(am_imp_exec);
    CPU_STEP(am_zp_1);
    CPU_STEP(am_zpx_1); CPU_STEP(am_zpx_2);
    CPU_STEP(am_zpy_1); CPU_STEP(am_zpy_2);
    CPU_STEP(am_abs_1); CPU_STEP(am_abs_2);
    CPU_STEP(am_abx_1); CPU_STEP(am_abx_2); CPU_STEP(am_abx_3_fixup);
    CPU_STEP(am_aby_1); CPU_STEP(am_aby_2); CPU_STEP(am_aby_3_fixup);
    CPU_STEP(am_izx_1); CPU_STEP(am_izx_2); CPU_STEP(am_izx_3); CPU_STEP(am_izx_4);
    CPU_STEP(am_izy_1); CPU_STEP(am_izy_2); CPU_STEP(am_izy_3); CPU_STEP(am_izy_4_fixup);
    CPU_STEP(am_rel_1); CPU_STEP(am_rel_2_taken); CPU_STEP(am_rel_3_pagefix);
    CPU_STEP(am_ind_1); CPU_STEP(am_ind_2); CPU_STEP(am_ind_3); CPU_STEP(am_ind_4);
    CPU_STEP(readAndExec_);
    CPU_STEP(rmw_dummyWrite); CPU_STEP(rmw_finalWrite);
    CPU_STEP(int1_dummy); CPU_STEP(int2_pushPCH); CPU_STEP(int3_pushPCL);
    CPU_STEP(int5_readLow); CPU_STEP(int6_readHigh);
    CPU_STEP(rti1_dummyRead); CPU_STEP(rti2_incS); CPU_STEP(rti3_pullP);
    CPU_STEP(rti4_pullPCL);   CPU_STEP(rti5_pullPCH);
    CPU_STEP(rts1_dummyRead); CPU_STEP(rts2_incS); CPU_STEP(rts3_pullPCL);
    CPU_STEP(rts4_pullPCH);   CPU_STEP(rts5_incPC);
    CPU_STEP(jsr1_readLow); CPU_STEP(jsr2_internal); CPU_STEP(jsr3_pushPCH);
    CPU_STEP(jsr4_pushPCL); CPU_STEP(jsr5_readHigh);
    CPU_STEP(pha1_dummy);
    CPU_STEP(php1_dummy);
    CPU_STEP(pla1_dummy); CPU_STEP(pla2_incS); CPU_STEP(pla3_pull);
    CPU_STEP(plp1_dummy); CPU_STEP(plp2_incS); CPU_STEP(plp3_pull);
    CPU_STEP(jmpAbs1_low); CPU_STEP(jmpAbs2_high);
    CPU_STEP(jam_loop);
    #undef CPU_STEP
    return "?";
}


