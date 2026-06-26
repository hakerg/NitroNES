#pragma once
#include "NESHeadlessSystem.h"
#include <algorithm>
#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

class TestRunner {
public:
    explicit TestRunner(const std::string& romPath)
        : nes(romPath) {
        nes.preStepHook  = [this]{ snapshotCpu(); };
        nes.postStepHook = [this]{ traceTick(); };
        nes.ppuStepHook  = [this](int){ tracePpuSub(); };
    }

    ~TestRunner() {
        closeTrace();
    }

    void loadSymbols(const std::string& path) {
        std::ifstream in(path);
        if (!in) return;
        std::string line;
        while (std::getline(in, line)) {
            // nesasm .fns format:   name = $hex
            // VICE/ld65 .lbl format: al hex .name
            if (line.size() >= 3 && line[0] == 'a' && line[1] == 'l' && line[2] == ' ') {
                auto sp = line.find(' ', 3);
                if (sp == std::string::npos) continue;
                std::string hex = line.substr(3, sp - 3);
                std::string name = line.substr(sp + 1);
                trim(name);
                if (!name.empty() && name[0] == '.') name.erase(0, 1);
                if (name.empty()) continue;
                if (name.rfind("LOCAL_MACRO_SYMBOL", 0) == 0) continue;
                if (name.rfind("__", 0) == 0) continue;
                uint32_t addr = (uint32_t)std::strtoul(hex.c_str(), nullptr, 16);
                if (addr <= 0xFFFF) symbolByAddr[(uint16_t)addr] = name;
                continue;
            }
            auto eq = line.find('=');
            if (eq == std::string::npos) continue;
            std::string name = line.substr(0, eq);
            std::string val  = line.substr(eq + 1);
            trim(name); trim(val);
            if (name.empty() || val.empty() || val[0] != '$') continue;
            uint32_t addr = (uint32_t)std::strtoul(val.c_str() + 1, nullptr, 16);
            symbolByAddr[(uint16_t)addr] = name;
        }
    }

    bool run(const std::vector<std::string>& commands) {
        for (const auto& cmd : commands) {
            std::cerr << "[nes_test] > " << cmd << "\n";
            if (!dispatch(cmd)) {
                std::cerr << "[nes_test] unknown command: " << cmd << "\n";
                return false;
            }
        }
        return true;
    }

private:
    NESHeadlessSystem nes;
    std::map<uint16_t, std::string> symbolByAddr;

    struct CpuSnap {
        uint16_t PC;
        uint8_t  A, X, Y, S, P;
    };
    CpuSnap preStep{};

    // tracer
    bool traceCpu = false, tracePpu = false, traceDma = false;
    std::FILE* traceFp = nullptr;
    std::string tracePath = "trace.log";

    // helpers ----------------------------------------------------------------
    static void trim(std::string& s) {
        auto isSp = [](unsigned char c){ return std::isspace(c); };
        while (!s.empty() && isSp(s.front())) s.erase(s.begin());
        while (!s.empty() && isSp(s.back()))  s.pop_back();
    }

    static std::string upper(std::string s) {
        for (auto& c : s) c = (char)std::toupper((unsigned char)c);
        return s;
    }

    static uint8_t buttonBit(const std::string& nameIn) {
        std::string n = upper(nameIn);
        if (n == "A")      return 0x80;
        if (n == "B")      return 0x40;
        if (n == "SELECT") return 0x20;
        if (n == "START")  return 0x10;
        if (n == "UP")     return 0x08;
        if (n == "DOWN")   return 0x04;
        if (n == "LEFT")   return 0x02;
        if (n == "RIGHT")  return 0x01;
        throw std::runtime_error("unknown button: " + nameIn);
    }

    static uint8_t parseButtons(const std::string& list) {
        uint8_t b = 0;
        std::string cur;
        for (char c : list) {
            if (c == ',' || c == '+') {
                if (!cur.empty()) { b |= buttonBit(cur); cur.clear(); }
            } else cur.push_back(c);
        }
        if (!cur.empty()) b |= buttonBit(cur);
        return b;
    }

    static uint32_t parseInt(const std::string& s) {
        if (s.size() > 2 && (s.substr(0, 2) == "0x" || s.substr(0, 2) == "0X"))
            return (uint32_t)std::strtoul(s.c_str() + 2, nullptr, 16);
        if (!s.empty() && s[0] == '$')
            return (uint32_t)std::strtoul(s.c_str() + 1, nullptr, 16);
        return (uint32_t)std::strtoul(s.c_str(), nullptr, 10);
    }

    static std::vector<std::string> splitArgs(const std::string& cmd, char sep) {
        std::vector<std::string> out;
        std::string cur;
        for (char c : cmd) {
            if (c == sep) { out.push_back(cur); cur.clear(); }
            else cur.push_back(c);
        }
        out.push_back(cur);
        return out;
    }

    // dispatch ---------------------------------------------------------------
    bool dispatch(const std::string& cmd) {
        if (cmd == "reset") { nes.reset(); return true; }
        if (cmd == "screen"){ printScreen(); return true; }

        if (cmd.size() >= 5 && cmd.compare(0, 3, "pad") == 0
            && (cmd[3] == '1' || cmd[3] == '2'))
        {
            int  port = cmd[3] - '1';
            char op   = cmd[4];
            if (op != '=' && op != '+' && op != '-') return false;
            return padCmd(port, op, cmd.substr(5));
        }

        auto parts = splitArgs(cmd, ':');
        const std::string& head = parts[0];

        if (head == "frames" && parts.size() == 2) {
            uint32_t n = parseInt(parts[1]);
            for (uint32_t i = 0; i < n; ++i) nes.tickFrame();
            return true;
        }
        if (head == "cycles" && parts.size() == 2) {
            nes.tickCycles(parseInt(parts[1]));
            return true;
        }
        if (head == "mem" && parts.size() == 3) {
            printMem((uint16_t)parseInt(parts[1]), parseInt(parts[2]));
            return true;
        }
        if (head == "trace-file" && parts.size() == 2) {
            tracePath = parts[1];
            closeTrace();
            return true;
        }
        if (head == "trace" && parts.size() == 3) {
            bool on = (parts[2] == "on" || parts[2] == "1");
            if      (parts[1] == "cpu") traceCpu = on;
            else if (parts[1] == "ppu") tracePpu = on;
            else if (parts[1] == "dma") traceDma = on;
            else return false;
            if (traceAny() && !traceFp) openTrace();
            return true;
        }
        return false;
    }

    bool padCmd(int port, char op, const std::string& arg) {
        uint8_t cur  = port == 0 ? nes.getController1() : nes.getController2();
        uint8_t mask = arg.empty() ? 0 : parseButtons(arg);

        if      (op == '=') cur = mask;
        else if (op == '+') cur |= mask;
        else if (op == '-') cur &= ~mask;
        else return false;

        if (port == 0) nes.setController1(cur); else nes.setController2(cur);
        return true;
    }

    // memory / screen --------------------------------------------------------
    void printScreen() {
        uint8_t tiles[960]{};
        nes.dumpNametable(tiles, 0);
        std::cout << "---- screen (NT0) frame=" << nes.frameNo()
                  << " cyc=" << nes.cycleNo() << " ----\n";
        for (int row = 0; row < 30; ++row) {
            std::string line;
            for (int col = 0; col < 32; ++col) {
                uint8_t t = tiles[row * 32 + col];
                line += (t >= 0x20 && t <= 0x7E) ? char(t) : ' ';
            }
            auto last = line.find_last_not_of(' ');
            if (last != std::string::npos)
                std::cout << line.substr(0, last + 1) << "\n";
            else
                std::cout << "\n";
        }
    }

    void printMem(uint16_t addr, uint32_t len) {
        std::cout << "---- mem $" << hex4(addr) << " len=" << len << " ----\n";
        for (uint32_t i = 0; i < len; i += 16) {
            std::printf("%04X: ", (uint16_t)(addr + i));
            for (uint32_t j = 0; j < 16 && i + j < len; ++j)
                std::printf("%02X ", nes.peekCPU((uint16_t)(addr + i + j)));
            std::printf("\n");
        }
    }

    // tracer -----------------------------------------------------------------
    bool traceAny() const { return traceCpu || tracePpu || traceDma; }

    void snapshotCpu() {
        auto& cpu = nes.getA2A03().getCPU();
        preStep = { cpu.PC, cpu.A, cpu.X, cpu.Y, cpu.S, cpu.P };
    }

    void openTrace() {
        traceFp = std::fopen(tracePath.c_str(), "w");
        if (!traceFp) {
            std::cerr << "[nes_test] cannot open trace file: " << tracePath << "\n";
            return;
        }
        std::cerr << "[nes_test] trace -> " << tracePath << "\n";
    }

    void closeTrace() {
        if (traceFp) { std::fclose(traceFp); traceFp = nullptr; }
    }

    void tracePpuSub() {
        if (!tracePpu) return;
        if (!traceFp) openTrace();
        if (!traceFp) return;
        auto& ppu = nes.getPPURef();
        std::fprintf(traceFp,
            "F=%llu CYC=%llu PPU[SL=%3d,CY=%3d] %-10s V=%04X T=%04X fX=%u W=%u "
            "CTRL=%02X MASK=%02X STAT=%02X OAMA=%02X SPR=%u NMI=%u%s\n",
            (unsigned long long)nes.frameNo(),
            (unsigned long long)nes.cycleNo(),
            (int)ppu.getScanline(), (int)ppu.getCycle(),
            ppuPhase(ppu.getScanline(), ppu.getCycle()),
            (unsigned)ppu.getVramAddr(),  (unsigned)ppu.getTramAddr(),
            (unsigned)ppu.getFineX(),     (unsigned)ppu.getWriteLatch(),
            (unsigned)ppu.getCtrl(),      (unsigned)ppu.getMask(),
            (unsigned)ppu.getStatus(),    (unsigned)ppu.getOamAddr(),
            (unsigned)ppu.getSpriteCount(),
            ppu.nmiLineLow() ? 1u : 0u,
            ppu.getFrameOdd() ? " ODD" : "");
    }

    static const char* ppuPhase(int sl, int cy) {
        if (sl == 261)                       return "PRE";
        if (sl >= 0 && sl <= 239) {
            if (cy == 0)                     return "IDLE";
            if (cy <= 256)                   return "BG-FETCH";
            if (cy <= 320)                   return "SPR-FETCH";
            if (cy <= 336)                   return "BG-PREFTCH";
            return "NT-DUMMY";
        }
        if (sl == 240)                       return "POST";
        if (sl >= 241 && sl <= 260)          return "VBLANK";
        return "?";
    }

    void traceTick() {
        if (!traceCpu && !traceDma) return;
        if (!traceFp) openTrace();
        if (!traceFp) return;

        auto& cpu = nes.getA2A03().getCPU();
        auto& dma = nes.getA2A03().getDMA();
        std::fprintf(traceFp, "F=%llu CYC=%llu",
                     (unsigned long long)nes.frameNo(),
                     (unsigned long long)nes.cycleNo());

        if (traceCpu) {
            const uint16_t addr = nes.getA2A03().getAddr();
            const uint8_t  data = nes.getA2A03().getBusData();
            const bool dmaOver = dma.overridesAddr();
            char rw = '?';
            if (dmaOver) {
                rw = (dma.actionId() == 2 /*OAMPut*/) ? 'W' : 'R';
            } else {
                rw = cpu.isRead() ? 'R' : 'W';
            }
            const char* addrName = ioRegName(addr);
            std::string addrSym  = addrName ? addrName : symbolExact(addr);
            char addrField[32];
            if (!addrSym.empty())
                std::snprintf(addrField, sizeof(addrField), "$%04X=%02X(%s)",
                              addr, data, addrSym.c_str());
            else
                std::snprintf(addrField, sizeof(addrField), "$%04X=%02X", addr, data);

            std::fprintf(traceFp,
                " PC=%04X A=%02X X=%02X Y=%02X S=%02X P=%s %c %-26s %-4s %-18s",
                preStep.PC, preStep.A, preStep.X, preStep.Y, preStep.S,
                flagStr(preStep.P).c_str(),
                rw, addrField,
                cpu.currentOpName(),
                cpu.currentStepName());
            const std::string sym = symbolNear(preStep.PC);
            if (!sym.empty()) std::fprintf(traceFp, " ; %s", sym.c_str());
        }

        if (traceDma) {
            static const char* dmcPh[] = { "Idle", "Halt", "Dummy", "Read" };
            static const char* oamPh[] = { "Idle", "Halt", "Xfer" };
            static const char* acts[]  = { "None", "OAMGet", "OAMPut", "DMCGet" };
            std::fprintf(traceFp, "  DMA:%s/%s %s @%04X",
                         oamPh[dma.oamPhaseId() & 3],
                         dmcPh[dma.dmcPhaseId() & 3],
                         acts[dma.actionId()    & 3],
                         (unsigned)dma.getAddr());
        }
        std::fprintf(traceFp, "\n");
    }

    static std::string flagStr(uint8_t p) {
        const char* names = "NV-BDIZC";
        std::string out(8, '-');
        for (int i = 0; i < 8; ++i) {
            bool set = (p >> (7 - i)) & 1;
            out[i] = set ? names[i] : (char)std::tolower((unsigned char)names[i]);
        }
        out[2] = (p & 0x20) ? 'U' : 'u'; // bit 5 always considered 'U'
        return out;
    }

    static std::string hex4(uint16_t v) {
        char b[8]; std::snprintf(b, sizeof(b), "%04X", v); return b;
    }

    std::string symbolNear(uint16_t pc) const {
        if (symbolByAddr.empty()) return {};
        auto it = symbolByAddr.upper_bound(pc);
        if (it == symbolByAddr.begin()) return {};
        --it;
        uint16_t off = pc - it->first;
        if (off > 0x40) return {};
        if (off == 0) return it->second;
        char buf[16]; std::snprintf(buf, sizeof(buf), "+%u", (unsigned)off);
        return it->second + buf;
    }

    std::string symbolExact(uint16_t addr) const {
        auto it = symbolByAddr.find(addr);
        return it == symbolByAddr.end() ? std::string{} : it->second;
    }

    static const char* ioRegName(uint16_t addr) {
        switch (addr) {
            case 0x2000: return "PPUCTRL";
            case 0x2001: return "PPUMASK";
            case 0x2002: return "PPUSTATUS";
            case 0x2003: return "OAMADDR";
            case 0x2004: return "OAMDATA";
            case 0x2005: return "PPUSCROLL";
            case 0x2006: return "PPUADDR";
            case 0x2007: return "PPUDATA";
            case 0x4000: return "SQ1_VOL";
            case 0x4001: return "SQ1_SWEEP";
            case 0x4002: return "SQ1_LO";
            case 0x4003: return "SQ1_HI";
            case 0x4004: return "SQ2_VOL";
            case 0x4005: return "SQ2_SWEEP";
            case 0x4006: return "SQ2_LO";
            case 0x4007: return "SQ2_HI";
            case 0x4008: return "TRI_LINEAR";
            case 0x400A: return "TRI_LO";
            case 0x400B: return "TRI_HI";
            case 0x400C: return "NOISE_VOL";
            case 0x400E: return "NOISE_LO";
            case 0x400F: return "NOISE_HI";
            case 0x4010: return "DMC_FREQ";
            case 0x4011: return "DMC_RAW";
            case 0x4012: return "DMC_START";
            case 0x4013: return "DMC_LEN";
            case 0x4014: return "OAMDMA";
            case 0x4015: return "APU_STATUS";
            case 0x4016: return "JOY1";
            case 0x4017: return "JOY2/FRAME";
            default:     return nullptr;
        }
    }
};












