#pragma once
#include <cstdint>

class ICPUBus {
public:
    virtual ~ICPUBus() = default;

    // Podstawowe operacje na magistrali
    virtual uint8_t cpuRead(uint16_t addr) = 0;
    virtual void    cpuWrite(uint16_t addr, uint8_t data) = 0;

    // Czyszczenie/potwierdzenie przerwań (zastępuje irqAck)
    virtual void    cpuIrqAck() = 0;
};

// ============================================================
//  CPU6502 - cycle-accurate continuation-passing FSM.
//
//  Kazda mikro-op = jeden cykl magistrali. Mikro-op ZWRACA Step
//  wskazujacy nastepna mikro-op (zamiast mutowac nextOp). tick():
//      nextOp = (this->*nextOp)().next;
// ============================================================
class CPU6502 {
public:
    // 6502 status flags
    static constexpr uint8_t FLAG_C = 0x01;
    static constexpr uint8_t FLAG_Z = 0x02;
    static constexpr uint8_t FLAG_I = 0x04;
    static constexpr uint8_t FLAG_D = 0x08;
    static constexpr uint8_t FLAG_B = 0x10;
    static constexpr uint8_t FLAG_U = 0x20;
    static constexpr uint8_t FLAG_V = 0x40;
    static constexpr uint8_t FLAG_N = 0x80;

    uint16_t PC = 0;
    uint8_t  S  = 0x00;
    uint8_t  A  = 0;
    uint8_t  X  = 0;
    uint8_t  Y  = 0;
    uint8_t  P  = FLAG_U | FLAG_B;

    uint64_t totalCycles = 0;

private:
    struct Step;
    using MicroOp = Step (CPU6502::*)();
    struct Step { MicroOp next; };

public:
    CPU6502(ICPUBus& busInterface) : bus(busInterface) {
        nextOp = &CPU6502::opFetch;
    }

    void reset() {
        S -= 3;
        P |= FLAG_I;

        uint16_t lo = bus.cpuRead(0xFFFC);
        uint16_t hi = bus.cpuRead(0xFFFD);
        PC = lo | (hi << 8);
        nextOp = &CPU6502::opFetch;
        nmiPending = false;
        prevNmiLineLow = false;
        irqLine = false;
        irqDetected = false;
        nmiDetected = false;
        interruptPending = false;
        currentInt = IntKind::None;
        stallCycles = 7;
    }

    void tick() {
        totalCycles++;

        nmiAtStartOfCycle = nmiDetected;
        irqAtStartOfCycle = irqDetected;

        if (stallCycles > 0) {
            // Degrader: Transformacja instrukcji SH* w zepsute odpowiedniki
            if (nextOp == &CPU6502::am_abx_2 || nextOp == &CPU6502::am_aby_2 || nextOp == &CPU6502::am_izy_3) {
                if (storeKind == StoreKind::SHX) storeKind = StoreKind::SHX_c3;
                else if (storeKind == StoreKind::SHY) storeKind = StoreKind::SHY_c3;
                else if (storeKind == StoreKind::SHA) storeKind = StoreKind::SHA_c3;
                else if (storeKind == StoreKind::SHS) storeKind = StoreKind::SHS_c3;
            }
            else if (nextOp == &CPU6502::am_abx_3_fixup || nextOp == &CPU6502::am_aby_3_fixup || nextOp == &CPU6502::am_izy_4_fixup) {
                if (storeKind == StoreKind::SHX) storeKind = StoreKind::SHX_c4;
                else if (storeKind == StoreKind::SHY) storeKind = StoreKind::SHY_c4;
                else if (storeKind == StoreKind::SHA) storeKind = StoreKind::SHA_c4;
                else if (storeKind == StoreKind::SHS) storeKind = StoreKind::SHS_c4;
            }

            stallCycles--;
            sampleInterruptLatches();
            return;
        }

        nextOp = (this->*nextOp)().next;

        sampleInterruptLatches();
    }

    void sampleInterruptLatches() {
        irqDetected = irqLine;
        nmiDetected = nmiPending;
    }

    void setNMILine(bool low) {
        if (low && !prevNmiLineLow) {
            nmiPending = true;
            nmiDetected = true;
        }
        prevNmiLineLow = low;
    }
    void setIRQ(bool level)   {
        irqLine = level;
        irqDetected = level;
    }

    void addStall(uint16_t n) { stallCycles += n; }
    bool isAtInstructionBoundary() const { return nextOp == &CPU6502::opFetch; }

private:
    ICPUBus& bus;

    MicroOp nextOp = nullptr;

    uint8_t opcode = 0;
    uint16_t addr = 0;
    uint16_t ptr = 0;
    uint8_t fetched = 0;
    uint8_t tmp = 0;
    bool pageCross = false;
    bool nmiPending = false;
    bool prevNmiLineLow = false; // poprzedni poziom /NMI (do edge detection)
    bool irqLine = false;
    bool irqDetected = false;
    bool nmiDetected = false;
    bool interruptPending = false;
    bool irqInhibitSnapshot = true; // wartosc FLAG_I z poczatku biezacej instrukcji
    enum class IntKind : uint8_t { None, SoftwareBRK, IRQ, NMI };
    IntKind currentInt = IntKind::None;
    bool nmiAtStartOfCycle = false;
    bool irqAtStartOfCycle = false;

    uint16_t intVector() const { return currentInt == IntKind::NMI ? 0xFFFA : 0xFFFE; }

    uint16_t stallCycles = 0;

    // ---- Helpery ----
    static Step STEP(MicroOp m) { return Step{ m }; }

    void pollInterrupts() {
        if (nmiAtStartOfCycle) interruptPending = true;
        else if (irqAtStartOfCycle && !irqInhibitSnapshot) interruptPending = true;
    }

    Step DONE() {
        pollInterrupts();
        return Step{ &CPU6502::opFetch };
    }
    Step DONE_NOPOLL() {
        return Step{ &CPU6502::opFetch };
    }

    void setZN(uint8_t v) {
        P = (P & ~(FLAG_Z | FLAG_N)) | (v == 0 ? FLAG_Z : 0) | (v & 0x80);
    }
    void push(uint8_t v) { bus.cpuWrite(0x0100 | S, v); S--; }
    uint8_t pull()       { S++; return bus.cpuRead(0x0100 | S); }

    // ============================================================
    //  Mikro-op'y
    // ============================================================
    Step opFetch() {
        if (interruptPending) {
            interruptPending = false;
            if (nmiPending) {
                nmiPending  = false;
                nmiDetected = false; // latch edge zerowany przy obsludze NMI
                currentInt  = IntKind::NMI;
            } else {
                currentInt = IntKind::IRQ;
                bus.cpuIrqAck();
            }
            opcode = 0x00;
            return STEP(&CPU6502::brk1_dummy);
        }
        irqInhibitSnapshot = (P & FLAG_I) != 0; // snapshot przed wykonaniem instrukcji
        opcode = bus.cpuRead(PC++);
        return decodeAndDispatch();
    }

    Step decodeAndDispatch();

    Step afterAddressing();
    Step readAndExec_();
    Step rmw_read();
    Step rmw_dummyWrite();
    Step rmw_finalWrite();
    Step writeStep_();

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

    Step brk1_dummy();
    Step brk2_pushPCH();
    Step brk3_pushPCL();
    Step brk4_pushP();
    Step brk5_readLow();
    Step brk6_readHigh();

    Step rti1_dummyRead(); Step rti2_incS(); Step rti3_pullP();
    Step rti4_pullPCL();   Step rti5_pullPCH();

    Step rts1_dummyRead(); Step rts2_incS(); Step rts3_pullPCL();
    Step rts4_pullPCH();   Step rts5_incPC();

    Step jsr1_readLow();  Step jsr2_internal(); Step jsr3_pushPCH();
    Step jsr4_pushPCL();  Step jsr5_readHigh();

    Step pha1_dummy(); Step pha2_push();
    Step php1_dummy(); Step php2_push();
    Step pla1_dummy(); Step pla2_incS(); Step pla3_pull();
    Step plp1_dummy(); Step plp2_incS(); Step plp3_pull();

    Step jmpAbs1_low(); Step jmpAbs2_high();

    // ---- Czysta egzekucja ----
    void execLDA() { A = fetched; setZN(A); }
    void execLDX() { X = fetched; setZN(X); }
    void execLDY() { Y = fetched; setZN(Y); }
    void execAND() { A &= fetched; setZN(A); }
    void execORA() { A |= fetched; setZN(A); }
    void execEOR() { A ^= fetched; setZN(A); }
    void execBIT() {
        uint8_t r = A & fetched;
        P = (P & ~(FLAG_Z | FLAG_N | FLAG_V))
            | (r == 0 ? FLAG_Z : 0) | (fetched & 0x80) | (fetched & 0x40);
    }
    void execADC() {
        uint16_t s = (uint16_t)A + fetched + ((P & FLAG_C) ? 1 : 0);
        uint8_t  r = (uint8_t)s;
        P = (P & ~(FLAG_C | FLAG_Z | FLAG_V | FLAG_N))
            | (s > 0xFF ? FLAG_C : 0) | (r == 0 ? FLAG_Z : 0)
            | (((~(A ^ fetched) & (A ^ r)) & 0x80) ? FLAG_V : 0) | (r & 0x80);
        A = r;
    }
    void execSBC() {
        uint8_t v = fetched ^ 0xFF;
        uint16_t s = (uint16_t)A + v + ((P & FLAG_C) ? 1 : 0);
        uint8_t  r = (uint8_t)s;
        P = (P & ~(FLAG_C | FLAG_Z | FLAG_V | FLAG_N))
            | (s > 0xFF ? FLAG_C : 0) | (r == 0 ? FLAG_Z : 0)
            | (((~(A ^ v) & (A ^ r)) & 0x80) ? FLAG_V : 0) | (r & 0x80);
        A = r;
    }
    void execCMP() { uint16_t s = (uint16_t)A - fetched; P = (P & ~(FLAG_C|FLAG_Z|FLAG_N)) | (A >= fetched ? FLAG_C : 0) | (((uint8_t)s) == 0 ? FLAG_Z : 0) | (s & 0x80); }
    void execCPX() { uint16_t s = (uint16_t)X - fetched; P = (P & ~(FLAG_C|FLAG_Z|FLAG_N)) | (X >= fetched ? FLAG_C : 0) | (((uint8_t)s) == 0 ? FLAG_Z : 0) | (s & 0x80); }
    void execCPY() { uint16_t s = (uint16_t)Y - fetched; P = (P & ~(FLAG_C|FLAG_Z|FLAG_N)) | (Y >= fetched ? FLAG_C : 0) | (((uint8_t)s) == 0 ? FLAG_Z : 0) | (s & 0x80); }

    void execASL_mem() { P = (P & ~FLAG_C) | ((fetched & 0x80) ? FLAG_C : 0); tmp = fetched << 1; setZN(tmp); }
    void execLSR_mem() { P = (P & ~FLAG_C) | ((fetched & 0x01) ? FLAG_C : 0); tmp = fetched >> 1; setZN(tmp); }
    void execROL_mem() { uint8_t c = (P & FLAG_C) ? 1 : 0; P = (P & ~FLAG_C) | ((fetched & 0x80) ? FLAG_C : 0); tmp = (fetched << 1) | c; setZN(tmp); }
    void execROR_mem() { uint8_t c = (P & FLAG_C) ? 0x80 : 0; P = (P & ~FLAG_C) | ((fetched & 0x01) ? FLAG_C : 0); tmp = (fetched >> 1) | c; setZN(tmp); }
    void execINC_mem() { tmp = fetched + 1; setZN(tmp); }
    void execDEC_mem() { tmp = fetched - 1; setZN(tmp); }

    void execASL_A() { P = (P & ~FLAG_C) | ((A & 0x80) ? FLAG_C : 0); A <<= 1; setZN(A); }
    void execLSR_A() { P = (P & ~FLAG_C) | ((A & 0x01) ? FLAG_C : 0); A >>= 1; setZN(A); }
    void execROL_A() { uint8_t c = (P & FLAG_C) ? 1 : 0; P = (P & ~FLAG_C) | ((A & 0x80) ? FLAG_C : 0); A = (A << 1) | c; setZN(A); }
    void execROR_A() { uint8_t c = (P & FLAG_C) ? 0x80 : 0; P = (P & ~FLAG_C) | ((A & 0x01) ? FLAG_C : 0); A = (A >> 1) | c; setZN(A); }

    // ---- Nieoficjalne (read/immediate) ----
    void execLAX() { A = fetched; X = fetched; setZN(A); }
    void execANC() {
        A &= fetched; setZN(A);
        P = (P & ~FLAG_C) | ((A & 0x80) ? FLAG_C : 0);
    }
    void execALR() {
        A &= fetched;
        P = (P & ~FLAG_C) | ((A & 0x01) ? FLAG_C : 0);
        A >>= 1; setZN(A);
    }
    void execARR() {
        A &= fetched;
        uint8_t c = (P & FLAG_C) ? 0x80 : 0;
        A = (A >> 1) | c;
        setZN(A);
        // C = bit6 wyniku, V = bit6 XOR bit5
        P = (P & ~(FLAG_C | FLAG_V))
            | ((A & 0x40) ? FLAG_C : 0)
            | (((A ^ (A << 1)) & 0x40) ? FLAG_V : 0);
    }
    // XAA/ANE i LXA s? niestabilne; powszechnie u?ywany model "magic constant"
    void execXAA() { A = (A | 0xEE) & X & fetched; setZN(A); }
    void execLXA() { A = (A | 0xFF) & fetched; X = A; setZN(A); }
    void execAXS() {
        uint16_t s = (uint16_t)(A & X) - fetched;
        X = (uint8_t)s;
        P = (P & ~(FLAG_C | FLAG_Z | FLAG_N))
            | (s < 0x100 ? FLAG_C : 0)
            | (X == 0 ? FLAG_Z : 0)
            | (X & 0x80);
    }
    void execLAS() {
        uint8_t v = fetched & S;
        A = v; X = v; S = v;
        setZN(v);
    }

    // ---- Nieoficjalne RMW (modyfikuj? pami??, potem operuj? na A) ----
    void execSLO() { execASL_mem(); A |= tmp; setZN(A); }
    void execRLA() { execROL_mem(); A &= tmp; setZN(A); }
    void execSRE() { execLSR_mem(); A ^= tmp; setZN(A); }
    void execRRA() { execROR_mem(); fetched = tmp; execADC(); }
    void execDCP() {
        execDEC_mem();
        uint8_t v = tmp;
        uint8_t r = A - v;
        P = (P & ~(FLAG_C | FLAG_Z | FLAG_N))
            | (A >= v ? FLAG_C : 0) | (r == 0 ? FLAG_Z : 0) | (r & 0x80);
    }
    void execISC() { execINC_mem(); fetched = tmp; execSBC(); }

    enum class ExecKind : uint8_t {
        LDA, LDX, LDY, AND_, ORA_, EOR_, BIT_, ADC_, SBC_, CMP_, CPX_, CPY_, NOP_,
        LAX_, ANC_, ALR_, ARR_, XAA_, AXS_, LAS_, LXA_
    };
    ExecKind execKind = ExecKind::NOP_;
    void doExecRead();

    enum class RmwKind : uint8_t {
        ASL, LSR, ROL, ROR, INC, DEC,
        SLO, RLA, SRE, RRA, DCP, ISC
    };
    RmwKind rmwKind = RmwKind::ASL;
    void doExecRmw();

    enum class StoreKind : uint8_t {
        STA, STX, STY, SAX,
        SHX, SHY, SHA, SHS,
        // -- Warianty sprzętowo zdegradowane przez uderzenie linii RDY --
        SHX_c3, SHY_c3, SHA_c3, SHS_c3, // Stall 2 cykle przed zapisem
        SHX_c4, SHY_c4, SHA_c4, SHS_c4  // Stall 1 cykl przed zapisem
    };

    StoreKind storeKind = StoreKind::STA;
    uint8_t storeValue() {
        switch (storeKind) {
            case StoreKind::STA: return A;
            case StoreKind::STX: return X;
            case StoreKind::STY: return Y;
            case StoreKind::SAX: return A & X;

            // Standardowe, zdrowe działanie:
            case StoreKind::SHX: return X & ((uint8_t)(origHigh + 1));
            case StoreKind::SHY: return Y & ((uint8_t)(origHigh + 1));
            case StoreKind::SHA: return A & X & ((uint8_t)(origHigh + 1));
            case StoreKind::SHS: S = A & X; return S & ((uint8_t)(origHigh + 1));

            // Quirk zepsutej wartości (gubi maskę H+1):
            case StoreKind::SHX_c3: case StoreKind::SHX_c4: return X;
            case StoreKind::SHY_c3: case StoreKind::SHY_c4: return Y;
            case StoreKind::SHA_c3: case StoreKind::SHA_c4: return A & X;
            case StoreKind::SHS_c3: case StoreKind::SHS_c4: S = A & X; return S;
        }
        return 0;
    }

    enum class AccessMode : uint8_t { READ, RMW, WRITE };
    AccessMode accessMode = AccessMode::READ;

    bool    branchTaken = false;
    uint8_t branchOffset = 0;

    // Wysoki bajt operandu PRZED dodaniem indeksu (dla SHX/SHY/SHA/SHS/TAS).
    uint8_t origHigh = 0;

    Step jam_loop();
};

// =====================================================================
inline CPU6502::Step CPU6502::afterAddressing() {
    switch (accessMode) {
        case AccessMode::READ:  return STEP(&CPU6502::readAndExec_);
        case AccessMode::RMW:   return STEP(&CPU6502::rmw_read);
        case AccessMode::WRITE: return STEP(&CPU6502::writeStep_);
    }
    return STEP(&CPU6502::opFetch);
}

inline CPU6502::Step CPU6502::readAndExec_() {
    fetched = bus.cpuRead(addr);
    doExecRead();
    return DONE();
}
inline void CPU6502::doExecRead() {
    switch (execKind) {
        case ExecKind::LDA: execLDA(); break;
        case ExecKind::LDX: execLDX(); break;
        case ExecKind::LDY: execLDY(); break;
        case ExecKind::AND_: execAND(); break;
        case ExecKind::ORA_: execORA(); break;
        case ExecKind::EOR_: execEOR(); break;
        case ExecKind::BIT_: execBIT(); break;
        case ExecKind::ADC_: execADC(); break;
        case ExecKind::SBC_: execSBC(); break;
        case ExecKind::CMP_: execCMP(); break;
        case ExecKind::CPX_: execCPX(); break;
        case ExecKind::CPY_: execCPY(); break;
        case ExecKind::NOP_: break;
        case ExecKind::LAX_: execLAX(); break;
        case ExecKind::ANC_: execANC(); break;
        case ExecKind::ALR_: execALR(); break;
        case ExecKind::ARR_: execARR(); break;
        case ExecKind::XAA_: execXAA(); break;
        case ExecKind::AXS_: execAXS(); break;
        case ExecKind::LAS_: execLAS(); break;
        case ExecKind::LXA_: execLXA(); break;
    }
}

inline CPU6502::Step CPU6502::rmw_read() {
    fetched = bus.cpuRead(addr);
    return STEP(&CPU6502::rmw_dummyWrite);
}
inline CPU6502::Step CPU6502::rmw_dummyWrite() {
    bus.cpuWrite(addr, fetched);
    doExecRmw();
    return STEP(&CPU6502::rmw_finalWrite);
}
inline CPU6502::Step CPU6502::rmw_finalWrite() {
    bus.cpuWrite(addr, tmp);
    return DONE();
}
inline void CPU6502::doExecRmw() {
    switch (rmwKind) {
        case RmwKind::ASL: execASL_mem(); break;
        case RmwKind::LSR: execLSR_mem(); break;
        case RmwKind::ROL: execROL_mem(); break;
        case RmwKind::ROR: execROR_mem(); break;
        case RmwKind::INC: execINC_mem(); break;
        case RmwKind::DEC: execDEC_mem(); break;
        case RmwKind::SLO: execSLO(); break;
        case RmwKind::RLA: execRLA(); break;
        case RmwKind::SRE: execSRE(); break;
        case RmwKind::RRA: execRRA(); break;
        case RmwKind::DCP: execDCP(); break;
        case RmwKind::ISC: execISC(); break;
    }
}

inline CPU6502::Step CPU6502::writeStep_() {
    uint8_t val = storeValue();

    // Sprawdzamy czy to jakakolwiek instrukcja SH* (zdrowa lub zepsuta)
    bool isAnySh = (storeKind >= StoreKind::SHX);

    // Sprawdzamy czy to specyficzny wariant stallu w cyklu 3 (chroni przed zepsuciem adresu)
    bool isShC3 = (storeKind >= StoreKind::SHX_c3 && storeKind <= StoreKind::SHS_c3);

    if (isAnySh) {
        // Psujemy adres TYLKO wtedy, gdy przekroczono stronę i sprzęt NIE został wstrzymany 2 cykle wcześniej
        if (pageCross && !isShC3) {
            addr = ((uint16_t)val << 8) | (addr & 0xFF);
        }
    }

    bus.cpuWrite(addr, val);
    return DONE();
}

inline CPU6502::Step CPU6502::am_imm_exec() {
    fetched = bus.cpuRead(PC++);
    doExecRead();
    return DONE();
}
inline CPU6502::Step CPU6502::am_imp_exec() {
    bus.cpuRead(PC);
    return DONE();
}

inline CPU6502::Step CPU6502::am_zp_1()  { addr = bus.cpuRead(PC++); return afterAddressing(); }
inline CPU6502::Step CPU6502::am_zpx_1() { ptr = bus.cpuRead(PC++); return STEP(&CPU6502::am_zpx_2); }
inline CPU6502::Step CPU6502::am_zpx_2() { bus.cpuRead(ptr); addr = (ptr + X) & 0xFF; return afterAddressing(); }
inline CPU6502::Step CPU6502::am_zpy_1() { ptr = bus.cpuRead(PC++); return STEP(&CPU6502::am_zpy_2); }
inline CPU6502::Step CPU6502::am_zpy_2() { bus.cpuRead(ptr); addr = (ptr + Y) & 0xFF; return afterAddressing(); }

inline CPU6502::Step CPU6502::am_abs_1() { addr = bus.cpuRead(PC++); return STEP(&CPU6502::am_abs_2); }
inline CPU6502::Step CPU6502::am_abs_2() { addr |= bus.cpuRead(PC++) << 8; return afterAddressing(); }

inline CPU6502::Step CPU6502::am_abx_1() { addr = bus.cpuRead(PC++); return STEP(&CPU6502::am_abx_2); }
inline CPU6502::Step CPU6502::am_abx_2() {
    uint8_t high = bus.cpuRead(PC++);
    origHigh = high;
    uint16_t base = (high << 8) | (addr & 0xFF);
    uint16_t eff  = base + X;
    pageCross = (eff & 0xFF00) != (base & 0xFF00);
    addr = eff;
    if (accessMode != AccessMode::READ || pageCross)
        return STEP(&CPU6502::am_abx_3_fixup);
    return afterAddressing();
}
inline CPU6502::Step CPU6502::am_abx_3_fixup() {
    bus.cpuRead(addr - (pageCross ? 0x100 : 0));
    return afterAddressing();
}

inline CPU6502::Step CPU6502::am_aby_1() { addr = bus.cpuRead(PC++); return STEP(&CPU6502::am_aby_2); }
inline CPU6502::Step CPU6502::am_aby_2() {
    uint8_t high = bus.cpuRead(PC++);
    origHigh = high;
    uint16_t base = (high << 8) | (addr & 0xFF);
    uint16_t eff  = base + Y;
    pageCross = (eff & 0xFF00) != (base & 0xFF00);
    addr = eff;
    if (accessMode != AccessMode::READ || pageCross)
        return STEP(&CPU6502::am_aby_3_fixup);
    return afterAddressing();
}
inline CPU6502::Step CPU6502::am_aby_3_fixup() {
    bus.cpuRead(addr - (pageCross ? 0x100 : 0));
    return afterAddressing();
}

inline CPU6502::Step CPU6502::am_izx_1() { ptr = bus.cpuRead(PC++); return STEP(&CPU6502::am_izx_2); }
inline CPU6502::Step CPU6502::am_izx_2() { bus.cpuRead(ptr); ptr = (ptr + X) & 0xFF; return STEP(&CPU6502::am_izx_3); }
inline CPU6502::Step CPU6502::am_izx_3() { addr = bus.cpuRead(ptr); return STEP(&CPU6502::am_izx_4); }
inline CPU6502::Step CPU6502::am_izx_4() { addr |= bus.cpuRead((ptr + 1) & 0xFF) << 8; return afterAddressing(); }

inline CPU6502::Step CPU6502::am_izy_1() { ptr = bus.cpuRead(PC++); return STEP(&CPU6502::am_izy_2); }
inline CPU6502::Step CPU6502::am_izy_2() { addr = bus.cpuRead(ptr); return STEP(&CPU6502::am_izy_3); }
inline CPU6502::Step CPU6502::am_izy_3() {
    uint16_t high = bus.cpuRead((ptr + 1) & 0xFF);
    origHigh = (uint8_t)high;
    uint16_t base = (high << 8) | (addr & 0xFF);
    uint16_t eff  = base + Y;
    pageCross = (eff & 0xFF00) != (base & 0xFF00);
    addr = eff;
    if (accessMode != AccessMode::READ || pageCross)
        return STEP(&CPU6502::am_izy_4_fixup);
    return afterAddressing();
}
inline CPU6502::Step CPU6502::am_izy_4_fixup() {
    bus.cpuRead(addr - (pageCross ? 0x100 : 0));
    return afterAddressing();
}

inline CPU6502::Step CPU6502::am_rel_1() {
    branchOffset = bus.cpuRead(PC++);
    // Cykl 2 (operand fetch) - spec: "interrupts are always polled before
    // the second CPU cycle (the operand fetch)". Polling musi zajsc dla
    // OBYDWU sciezek (taken i not-taken). Dla not-taken DONE() i polluje,
    // i konczy. Dla taken polling rozdzielamy od koncowki, bo instrukcja
    // trwa jeszcze 1-2 cykle, ale cykl 3 (ostatni przy non-page-cross)
    // sam w sobie pollingu juz NIE wykona (quirk testowany przez
    // 5-branch_delays_irq/test_branch_taken).
    if (!branchTaken) return DONE();
    pollInterrupts();
    return STEP(&CPU6502::am_rel_2_taken);
}
inline CPU6502::Step CPU6502::am_rel_2_taken() {
    bus.cpuRead(PC);
    uint16_t oldPC = PC;
    uint8_t  oldL  = PC & 0xFF;
    uint8_t  newL  = oldL + branchOffset;
    PC = (oldPC & 0xFF00) | newL;
    bool cross = ((int8_t)branchOffset < 0) ? (newL > oldL) : (newL < oldL);
    // Cykl 3 taken-branch:
    //  * non-page-cross (3-cyklowy taken): IRQ na tym cyklu jest
    //    IGNOROWANY - to wlasnie quirk testowany w
    //    5-branch_delays_irq/test_branch_taken. DONE_NOPOLL konczy
    //    instrukcje BEZ pollingu.
    //  * page-cross: polling odbywa sie "przed PCH fixup", co w naszym
    //    modelu z 1-cyklowym latchem detektora oznacza sample na
    //    poczatku cyklu 4 (am_rel_3_pagefix.DONE()) - bo sygnal IRQ
    //    z φ2 cyklu 3 jest widoczny w φ1 cyklu 4.
    if (!cross) return DONE_NOPOLL();
    pageCross = ((int8_t)branchOffset < 0);
    return STEP(&CPU6502::am_rel_3_pagefix);
}
inline CPU6502::Step CPU6502::am_rel_3_pagefix() {
    bus.cpuRead(PC);
    PC += pageCross ? (uint16_t)(-0x100) : 0x100;
    // Cykl 4 (PCH fixup) - polling przez DONE(): snapshot z poczatku
    // tego cyklu odpowiada stanowi IRQ z φ2 cyklu 3 - zgodnie ze spec
    // "interrupts are polled before the PCH fixup cycle".
    return DONE();
}

inline CPU6502::Step CPU6502::am_ind_1() { ptr = bus.cpuRead(PC++); return STEP(&CPU6502::am_ind_2); }
inline CPU6502::Step CPU6502::am_ind_2() { ptr |= bus.cpuRead(PC++) << 8; return STEP(&CPU6502::am_ind_3); }
inline CPU6502::Step CPU6502::am_ind_3() { addr = bus.cpuRead(ptr); return STEP(&CPU6502::am_ind_4); }
inline CPU6502::Step CPU6502::am_ind_4() {
    uint16_t hiAddr = (ptr & 0xFF00) | ((ptr + 1) & 0xFF); // sprzetowy bug JMP (ind)
    addr |= bus.cpuRead(hiAddr) << 8;
    PC = addr;
    return DONE();
}

inline CPU6502::Step CPU6502::jmpAbs1_low()  { addr = bus.cpuRead(PC++); return STEP(&CPU6502::jmpAbs2_high); }
inline CPU6502::Step CPU6502::jmpAbs2_high() { addr |= bus.cpuRead(PC++) << 8; PC = addr; return DONE(); }

// ---- BRK / IRQ / NMI ----
inline CPU6502::Step CPU6502::brk1_dummy() {
    if (currentInt == IntKind::SoftwareBRK) bus.cpuRead(PC++);
    else                                    bus.cpuRead(PC);
    return STEP(&CPU6502::brk2_pushPCH);
}
inline CPU6502::Step CPU6502::brk2_pushPCH() { push((PC >> 8) & 0xFF); return STEP(&CPU6502::brk3_pushPCL); }
inline CPU6502::Step CPU6502::brk3_pushPCL() { push(PC & 0xFF); return STEP(&CPU6502::brk4_pushP); }
inline CPU6502::Step CPU6502::brk4_pushP() {
    bool wasBRK = (currentInt == IntKind::SoftwareBRK);

    // Sprawdzamy snapshot z początku tego konkretnego (piątego) cyklu
    if (currentInt != IntKind::NMI && nmiAtStartOfCycle) {
        nmiPending = false;
        currentInt = IntKind::NMI;
    }

    uint8_t pushP = P | FLAG_U | (wasBRK ? FLAG_B : 0);
    push(pushP);
    P |= FLAG_I;
    irqInhibitSnapshot = true;
    return STEP(&CPU6502::brk5_readLow);
}
inline CPU6502::Step CPU6502::brk5_readLow()  { addr = bus.cpuRead(intVector()); return STEP(&CPU6502::brk6_readHigh); }
inline CPU6502::Step CPU6502::brk6_readHigh() {
    addr |= bus.cpuRead(intVector() + 1) << 8;
    PC = addr;
    currentInt = IntKind::None;
    // Spec: po zakonczeniu sekwencji przerwania (BRK/IRQ/NMI) co najmniej
    // jedna instrukcja handlera musi sie wykonac przed kolejnym
    // przerwaniem. Polling na koncu cyklu 7 jest pomijany - patrz
    // 2-nmi_and_brk: "NMI after SEC at beginning of IRQ handler" oczekuje
    // 27 36 00, czyli SEC najpierw, NMI dopiero potem.
    return DONE_NOPOLL();
}

inline CPU6502::Step CPU6502::rti1_dummyRead() { bus.cpuRead(PC); return STEP(&CPU6502::rti2_incS); }
inline CPU6502::Step CPU6502::rti2_incS()      { bus.cpuRead(0x0100 | S); return STEP(&CPU6502::rti3_pullP); }
inline CPU6502::Step CPU6502::rti3_pullP()     { P = (pull() & ~FLAG_B) | FLAG_U; irqInhibitSnapshot = (P & FLAG_I) != 0; return STEP(&CPU6502::rti4_pullPCL); }
inline CPU6502::Step CPU6502::rti4_pullPCL()   { addr = pull(); return STEP(&CPU6502::rti5_pullPCH); }
inline CPU6502::Step CPU6502::rti5_pullPCH()   { addr |= pull() << 8; PC = addr; return DONE(); }

inline CPU6502::Step CPU6502::rts1_dummyRead() { bus.cpuRead(PC); return STEP(&CPU6502::rts2_incS); }
inline CPU6502::Step CPU6502::rts2_incS()      { bus.cpuRead(0x0100 | S); return STEP(&CPU6502::rts3_pullPCL); }
inline CPU6502::Step CPU6502::rts3_pullPCL()   { addr = pull(); return STEP(&CPU6502::rts4_pullPCH); }
inline CPU6502::Step CPU6502::rts4_pullPCH()   { addr |= pull() << 8; PC = addr; return STEP(&CPU6502::rts5_incPC); }
inline CPU6502::Step CPU6502::rts5_incPC()     { bus.cpuRead(PC); PC++; return DONE(); }

inline CPU6502::Step CPU6502::jsr1_readLow()  { addr = bus.cpuRead(PC++); return STEP(&CPU6502::jsr2_internal); }
inline CPU6502::Step CPU6502::jsr2_internal() { bus.cpuRead(0x0100 | S); return STEP(&CPU6502::jsr3_pushPCH); }
inline CPU6502::Step CPU6502::jsr3_pushPCH()  { push((PC >> 8) & 0xFF); return STEP(&CPU6502::jsr4_pushPCL); }
inline CPU6502::Step CPU6502::jsr4_pushPCL()  { push(PC & 0xFF); return STEP(&CPU6502::jsr5_readHigh); }
inline CPU6502::Step CPU6502::jsr5_readHigh() { addr |= bus.cpuRead(PC) << 8; PC = addr; return DONE(); }

inline CPU6502::Step CPU6502::pha1_dummy() { bus.cpuRead(PC); return STEP(&CPU6502::pha2_push); }
inline CPU6502::Step CPU6502::pha2_push()  { push(A); return DONE(); }
inline CPU6502::Step CPU6502::php1_dummy() { bus.cpuRead(PC); return STEP(&CPU6502::php2_push); }
inline CPU6502::Step CPU6502::php2_push()  { push(P | FLAG_B | FLAG_U); return DONE(); }
inline CPU6502::Step CPU6502::pla1_dummy() { bus.cpuRead(PC); return STEP(&CPU6502::pla2_incS); }
inline CPU6502::Step CPU6502::pla2_incS()  { bus.cpuRead(0x0100 | S); return STEP(&CPU6502::pla3_pull); }
inline CPU6502::Step CPU6502::pla3_pull()  { A = pull(); setZN(A); return DONE(); }
inline CPU6502::Step CPU6502::plp1_dummy() { bus.cpuRead(PC); return STEP(&CPU6502::plp2_incS); }
inline CPU6502::Step CPU6502::plp2_incS()  { bus.cpuRead(0x0100 | S); return STEP(&CPU6502::plp3_pull); }
inline CPU6502::Step CPU6502::plp3_pull()  { P = (pull() & ~FLAG_B) | FLAG_U; return DONE(); }

inline CPU6502::Step CPU6502::jam_loop() {
    // KIL/JAM/HLT - CPU zawiesza si? na zawsze.
    PC--; // utrzymuj PC na opkodzie JAM
    return STEP(&CPU6502::jam_loop);
}

// =====================================================================
inline CPU6502::Step CPU6502::decodeAndDispatch() {
    branchTaken = false;
    accessMode  = AccessMode::READ;
    execKind    = ExecKind::NOP_;

    auto setRead  = [&](ExecKind k, MicroOp am1) { execKind = k; accessMode = AccessMode::READ;  return STEP(am1); };
    auto setWrite = [&](StoreKind k, MicroOp am1){ storeKind = k; accessMode = AccessMode::WRITE; return STEP(am1); };
    auto setRmw   = [&](RmwKind k, MicroOp am1)  { rmwKind = k;  accessMode = AccessMode::RMW;   return STEP(am1); };
    auto branch   = [&](bool cond)               { branchTaken = cond; return STEP(&CPU6502::am_rel_1); };
    auto implied  = [&](auto fn)                 { fn(); return STEP(&CPU6502::am_imp_exec); };

    switch (opcode) {
    case 0xA9: execKind = ExecKind::LDA; return STEP(&CPU6502::am_imm_exec);
    case 0xA5: return setRead(ExecKind::LDA, &CPU6502::am_zp_1);
    case 0xB5: return setRead(ExecKind::LDA, &CPU6502::am_zpx_1);
    case 0xAD: return setRead(ExecKind::LDA, &CPU6502::am_abs_1);
    case 0xBD: return setRead(ExecKind::LDA, &CPU6502::am_abx_1);
    case 0xB9: return setRead(ExecKind::LDA, &CPU6502::am_aby_1);
    case 0xA1: return setRead(ExecKind::LDA, &CPU6502::am_izx_1);
    case 0xB1: return setRead(ExecKind::LDA, &CPU6502::am_izy_1);

    case 0xA2: execKind = ExecKind::LDX; return STEP(&CPU6502::am_imm_exec);
    case 0xA6: return setRead(ExecKind::LDX, &CPU6502::am_zp_1);
    case 0xB6: return setRead(ExecKind::LDX, &CPU6502::am_zpy_1);
    case 0xAE: return setRead(ExecKind::LDX, &CPU6502::am_abs_1);
    case 0xBE: return setRead(ExecKind::LDX, &CPU6502::am_aby_1);

    case 0xA0: execKind = ExecKind::LDY; return STEP(&CPU6502::am_imm_exec);
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

    case 0xAA: return implied([&]{ X = A; setZN(X); });
    case 0xA8: return implied([&]{ Y = A; setZN(Y); });
    case 0xBA: return implied([&]{ X = S; setZN(X); });
    case 0x8A: return implied([&]{ A = X; setZN(A); });
    case 0x9A: return implied([&]{ S = X; });
    case 0x98: return implied([&]{ A = Y; setZN(A); });

    case 0x48: return STEP(&CPU6502::pha1_dummy);
    case 0x08: return STEP(&CPU6502::php1_dummy);
    case 0x68: return STEP(&CPU6502::pla1_dummy);
    case 0x28: return STEP(&CPU6502::plp1_dummy);

    case 0x29: execKind = ExecKind::AND_; return STEP(&CPU6502::am_imm_exec);
    case 0x25: return setRead(ExecKind::AND_, &CPU6502::am_zp_1);
    case 0x35: return setRead(ExecKind::AND_, &CPU6502::am_zpx_1);
    case 0x2D: return setRead(ExecKind::AND_, &CPU6502::am_abs_1);
    case 0x3D: return setRead(ExecKind::AND_, &CPU6502::am_abx_1);
    case 0x39: return setRead(ExecKind::AND_, &CPU6502::am_aby_1);
    case 0x21: return setRead(ExecKind::AND_, &CPU6502::am_izx_1);
    case 0x31: return setRead(ExecKind::AND_, &CPU6502::am_izy_1);

    case 0x09: execKind = ExecKind::ORA_; return STEP(&CPU6502::am_imm_exec);
    case 0x05: return setRead(ExecKind::ORA_, &CPU6502::am_zp_1);
    case 0x15: return setRead(ExecKind::ORA_, &CPU6502::am_zpx_1);
    case 0x0D: return setRead(ExecKind::ORA_, &CPU6502::am_abs_1);
    case 0x1D: return setRead(ExecKind::ORA_, &CPU6502::am_abx_1);
    case 0x19: return setRead(ExecKind::ORA_, &CPU6502::am_aby_1);
    case 0x01: return setRead(ExecKind::ORA_, &CPU6502::am_izx_1);
    case 0x11: return setRead(ExecKind::ORA_, &CPU6502::am_izy_1);

    case 0x49: execKind = ExecKind::EOR_; return STEP(&CPU6502::am_imm_exec);
    case 0x45: return setRead(ExecKind::EOR_, &CPU6502::am_zp_1);
    case 0x55: return setRead(ExecKind::EOR_, &CPU6502::am_zpx_1);
    case 0x4D: return setRead(ExecKind::EOR_, &CPU6502::am_abs_1);
    case 0x5D: return setRead(ExecKind::EOR_, &CPU6502::am_abx_1);
    case 0x59: return setRead(ExecKind::EOR_, &CPU6502::am_aby_1);
    case 0x41: return setRead(ExecKind::EOR_, &CPU6502::am_izx_1);
    case 0x51: return setRead(ExecKind::EOR_, &CPU6502::am_izy_1);

    case 0x24: return setRead(ExecKind::BIT_, &CPU6502::am_zp_1);
    case 0x2C: return setRead(ExecKind::BIT_, &CPU6502::am_abs_1);

    case 0x69: execKind = ExecKind::ADC_; return STEP(&CPU6502::am_imm_exec);
    case 0x65: return setRead(ExecKind::ADC_, &CPU6502::am_zp_1);
    case 0x75: return setRead(ExecKind::ADC_, &CPU6502::am_zpx_1);
    case 0x6D: return setRead(ExecKind::ADC_, &CPU6502::am_abs_1);
    case 0x7D: return setRead(ExecKind::ADC_, &CPU6502::am_abx_1);
    case 0x79: return setRead(ExecKind::ADC_, &CPU6502::am_aby_1);
    case 0x61: return setRead(ExecKind::ADC_, &CPU6502::am_izx_1);
    case 0x71: return setRead(ExecKind::ADC_, &CPU6502::am_izy_1);

    case 0xE9: case 0xEB: execKind = ExecKind::SBC_; return STEP(&CPU6502::am_imm_exec);
    case 0xE5: return setRead(ExecKind::SBC_, &CPU6502::am_zp_1);
    case 0xF5: return setRead(ExecKind::SBC_, &CPU6502::am_zpx_1);
    case 0xED: return setRead(ExecKind::SBC_, &CPU6502::am_abs_1);
    case 0xFD: return setRead(ExecKind::SBC_, &CPU6502::am_abx_1);
    case 0xF9: return setRead(ExecKind::SBC_, &CPU6502::am_aby_1);
    case 0xE1: return setRead(ExecKind::SBC_, &CPU6502::am_izx_1);
    case 0xF1: return setRead(ExecKind::SBC_, &CPU6502::am_izy_1);

    case 0xC9: execKind = ExecKind::CMP_; return STEP(&CPU6502::am_imm_exec);
    case 0xC5: return setRead(ExecKind::CMP_, &CPU6502::am_zp_1);
    case 0xD5: return setRead(ExecKind::CMP_, &CPU6502::am_zpx_1);
    case 0xCD: return setRead(ExecKind::CMP_, &CPU6502::am_abs_1);
    case 0xDD: return setRead(ExecKind::CMP_, &CPU6502::am_abx_1);
    case 0xD9: return setRead(ExecKind::CMP_, &CPU6502::am_aby_1);
    case 0xC1: return setRead(ExecKind::CMP_, &CPU6502::am_izx_1);
    case 0xD1: return setRead(ExecKind::CMP_, &CPU6502::am_izy_1);

    case 0xE0: execKind = ExecKind::CPX_; return STEP(&CPU6502::am_imm_exec);
    case 0xE4: return setRead(ExecKind::CPX_, &CPU6502::am_zp_1);
    case 0xEC: return setRead(ExecKind::CPX_, &CPU6502::am_abs_1);

    case 0xC0: execKind = ExecKind::CPY_; return STEP(&CPU6502::am_imm_exec);
    case 0xC4: return setRead(ExecKind::CPY_, &CPU6502::am_zp_1);
    case 0xCC: return setRead(ExecKind::CPY_, &CPU6502::am_abs_1);

    case 0xE8: return implied([&]{ X++; setZN(X); });
    case 0xC8: return implied([&]{ Y++; setZN(Y); });
    case 0xCA: return implied([&]{ X--; setZN(X); });
    case 0x88: return implied([&]{ Y--; setZN(Y); });

    case 0xE6: return setRmw(RmwKind::INC, &CPU6502::am_zp_1);
    case 0xF6: return setRmw(RmwKind::INC, &CPU6502::am_zpx_1);
    case 0xEE: return setRmw(RmwKind::INC, &CPU6502::am_abs_1);
    case 0xFE: return setRmw(RmwKind::INC, &CPU6502::am_abx_1);
    case 0xC6: return setRmw(RmwKind::DEC, &CPU6502::am_zp_1);
    case 0xD6: return setRmw(RmwKind::DEC, &CPU6502::am_zpx_1);
    case 0xCE: return setRmw(RmwKind::DEC, &CPU6502::am_abs_1);
    case 0xDE: return setRmw(RmwKind::DEC, &CPU6502::am_abx_1);

    case 0x0A: return implied([&]{ execASL_A(); });
    case 0x4A: return implied([&]{ execLSR_A(); });
    case 0x2A: return implied([&]{ execROL_A(); });
    case 0x6A: return implied([&]{ execROR_A(); });

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

    case 0x18: return implied([&]{ P &= ~FLAG_C; });
    case 0x38: return implied([&]{ P |=  FLAG_C; });
    case 0x58: return implied([&]{ P &= ~FLAG_I; });
    case 0x78: return implied([&]{ P |=  FLAG_I; });
    case 0xB8: return implied([&]{ P &= ~FLAG_V; });
    case 0xD8: return implied([&]{ P &= ~FLAG_D; });
    case 0xF8: return implied([&]{ P |=  FLAG_D; });

    case 0x10: return branch(!(P & FLAG_N));
    case 0x30: return branch( (P & FLAG_N));
    case 0x50: return branch(!(P & FLAG_V));
    case 0x70: return branch( (P & FLAG_V));
    case 0x90: return branch(!(P & FLAG_C));
    case 0xB0: return branch( (P & FLAG_C));
    case 0xD0: return branch(!(P & FLAG_Z));
    case 0xF0: return branch( (P & FLAG_Z));

    case 0x4C: return STEP(&CPU6502::jmpAbs1_low);
    case 0x6C: return STEP(&CPU6502::am_ind_1);
    case 0x20: return STEP(&CPU6502::jsr1_readLow);
    case 0x60: return STEP(&CPU6502::rts1_dummyRead);
    case 0x40: return STEP(&CPU6502::rti1_dummyRead);

    case 0x00:
        currentInt = IntKind::SoftwareBRK;
        return STEP(&CPU6502::brk1_dummy);

    case 0xEA: return implied([&]{});

    // ================================================================
    //                NIEOFICJALNE / NIEUDOKUMENTOWANE OPKODY
    // ================================================================

    // ---- NOP-y 1-bajtowe (implied) ----
    case 0x1A: case 0x3A: case 0x5A: case 0x7A: case 0xDA: case 0xFA:
        return implied([&]{});

    // ---- DOP / SKB (NOP z operandem zero-page lub immediate, 2-bajtowe) ----
    case 0x80: case 0x82: case 0x89: case 0xC2: case 0xE2:
        execKind = ExecKind::NOP_;
        return STEP(&CPU6502::am_imm_exec);
    case 0x04: case 0x44: case 0x64:
        return setRead(ExecKind::NOP_, &CPU6502::am_zp_1);
    case 0x14: case 0x34: case 0x54: case 0x74: case 0xD4: case 0xF4:
        return setRead(ExecKind::NOP_, &CPU6502::am_zpx_1);

    // ---- TOP / SKW (NOP 3-bajtowy, abs / abs,X z page-cross) ----
    case 0x0C:
        return setRead(ExecKind::NOP_, &CPU6502::am_abs_1);
    case 0x1C: case 0x3C: case 0x5C: case 0x7C: case 0xDC: case 0xFC:
        return setRead(ExecKind::NOP_, &CPU6502::am_abx_1);

    // ---- LAX ----
    case 0xA7: return setRead(ExecKind::LAX_, &CPU6502::am_zp_1);
    case 0xB7: return setRead(ExecKind::LAX_, &CPU6502::am_zpy_1);
    case 0xAF: return setRead(ExecKind::LAX_, &CPU6502::am_abs_1);
    case 0xBF: return setRead(ExecKind::LAX_, &CPU6502::am_aby_1);
    case 0xA3: return setRead(ExecKind::LAX_, &CPU6502::am_izx_1);
    case 0xB3: return setRead(ExecKind::LAX_, &CPU6502::am_izy_1);
    case 0xAB: execKind = ExecKind::LXA_; return STEP(&CPU6502::am_imm_exec);

    // ---- SAX (A & X -> mem) ----
    case 0x87: return setWrite(StoreKind::SAX, &CPU6502::am_zp_1);
    case 0x97: return setWrite(StoreKind::SAX, &CPU6502::am_zpy_1);
    case 0x8F: return setWrite(StoreKind::SAX, &CPU6502::am_abs_1);
    case 0x83: return setWrite(StoreKind::SAX, &CPU6502::am_izx_1);

    // ---- SBC nieoficjalne (alias #imm 0xEB ju? w SBC; ?adne inne nie wymagaj? dodatku) ----

    // ---- ANC / ALR / ARR / XAA / AXS ----
    case 0x0B: case 0x2B: execKind = ExecKind::ANC_; return STEP(&CPU6502::am_imm_exec);
    case 0x4B:            execKind = ExecKind::ALR_; return STEP(&CPU6502::am_imm_exec);
    case 0x6B:            execKind = ExecKind::ARR_; return STEP(&CPU6502::am_imm_exec);
    case 0x8B:            execKind = ExecKind::XAA_; return STEP(&CPU6502::am_imm_exec);
    case 0xCB:            execKind = ExecKind::AXS_; return STEP(&CPU6502::am_imm_exec);

    // ---- LAS (mem & S -> A,X,S) ----
    case 0xBB: return setRead(ExecKind::LAS_, &CPU6502::am_aby_1);

    // ---- SLO = ASL + ORA ----
    case 0x07: return setRmw(RmwKind::SLO, &CPU6502::am_zp_1);
    case 0x17: return setRmw(RmwKind::SLO, &CPU6502::am_zpx_1);
    case 0x0F: return setRmw(RmwKind::SLO, &CPU6502::am_abs_1);
    case 0x1F: return setRmw(RmwKind::SLO, &CPU6502::am_abx_1);
    case 0x1B: return setRmw(RmwKind::SLO, &CPU6502::am_aby_1);
    case 0x03: return setRmw(RmwKind::SLO, &CPU6502::am_izx_1);
    case 0x13: return setRmw(RmwKind::SLO, &CPU6502::am_izy_1);

    // ---- RLA = ROL + AND ----
    case 0x27: return setRmw(RmwKind::RLA, &CPU6502::am_zp_1);
    case 0x37: return setRmw(RmwKind::RLA, &CPU6502::am_zpx_1);
    case 0x2F: return setRmw(RmwKind::RLA, &CPU6502::am_abs_1);
    case 0x3F: return setRmw(RmwKind::RLA, &CPU6502::am_abx_1);
    case 0x3B: return setRmw(RmwKind::RLA, &CPU6502::am_aby_1);
    case 0x23: return setRmw(RmwKind::RLA, &CPU6502::am_izx_1);
    case 0x33: return setRmw(RmwKind::RLA, &CPU6502::am_izy_1);

    // ---- SRE = LSR + EOR ----
    case 0x47: return setRmw(RmwKind::SRE, &CPU6502::am_zp_1);
    case 0x57: return setRmw(RmwKind::SRE, &CPU6502::am_zpx_1);
    case 0x4F: return setRmw(RmwKind::SRE, &CPU6502::am_abs_1);
    case 0x5F: return setRmw(RmwKind::SRE, &CPU6502::am_abx_1);
    case 0x5B: return setRmw(RmwKind::SRE, &CPU6502::am_aby_1);
    case 0x43: return setRmw(RmwKind::SRE, &CPU6502::am_izx_1);
    case 0x53: return setRmw(RmwKind::SRE, &CPU6502::am_izy_1);

    // ---- RRA = ROR + ADC ----
    case 0x67: return setRmw(RmwKind::RRA, &CPU6502::am_zp_1);
    case 0x77: return setRmw(RmwKind::RRA, &CPU6502::am_zpx_1);
    case 0x6F: return setRmw(RmwKind::RRA, &CPU6502::am_abs_1);
    case 0x7F: return setRmw(RmwKind::RRA, &CPU6502::am_abx_1);
    case 0x7B: return setRmw(RmwKind::RRA, &CPU6502::am_aby_1);
    case 0x63: return setRmw(RmwKind::RRA, &CPU6502::am_izx_1);
    case 0x73: return setRmw(RmwKind::RRA, &CPU6502::am_izy_1);

    // ---- DCP = DEC + CMP ----
    case 0xC7: return setRmw(RmwKind::DCP, &CPU6502::am_zp_1);
    case 0xD7: return setRmw(RmwKind::DCP, &CPU6502::am_zpx_1);
    case 0xCF: return setRmw(RmwKind::DCP, &CPU6502::am_abs_1);
    case 0xDF: return setRmw(RmwKind::DCP, &CPU6502::am_abx_1);
    case 0xDB: return setRmw(RmwKind::DCP, &CPU6502::am_aby_1);
    case 0xC3: return setRmw(RmwKind::DCP, &CPU6502::am_izx_1);
    case 0xD3: return setRmw(RmwKind::DCP, &CPU6502::am_izy_1);

    // ---- ISC / ISB / INS = INC + SBC ----
    case 0xE7: return setRmw(RmwKind::ISC, &CPU6502::am_zp_1);
    case 0xF7: return setRmw(RmwKind::ISC, &CPU6502::am_zpx_1);
    case 0xEF: return setRmw(RmwKind::ISC, &CPU6502::am_abs_1);
    case 0xFF: return setRmw(RmwKind::ISC, &CPU6502::am_abx_1);
    case 0xFB: return setRmw(RmwKind::ISC, &CPU6502::am_aby_1);
    case 0xE3: return setRmw(RmwKind::ISC, &CPU6502::am_izx_1);
    case 0xF3: return setRmw(RmwKind::ISC, &CPU6502::am_izy_1);

    // ---- SH* (niestabilne store'y; uproszczony, ale popularny model) ----
    case 0x9C: return setWrite(StoreKind::SHY, &CPU6502::am_abx_1); // SHY abs,X
    case 0x9E: return setWrite(StoreKind::SHX, &CPU6502::am_aby_1); // SHX abs,Y
    case 0x9F: return setWrite(StoreKind::SHA, &CPU6502::am_aby_1); // SHA / AHX abs,Y
    case 0x93: return setWrite(StoreKind::SHA, &CPU6502::am_izy_1); // SHA / AHX (zp),Y
    case 0x9B: return setWrite(StoreKind::SHS, &CPU6502::am_aby_1); // SHS / TAS abs,Y

    // ---- JAM / KIL / HLT ----
    case 0x02: case 0x12: case 0x22: case 0x32: case 0x42: case 0x52:
    case 0x62: case 0x72: case 0x92: case 0xB2: case 0xD2: case 0xF2:
        return STEP(&CPU6502::jam_loop);

    default:   return implied([&]{});
    }
}
