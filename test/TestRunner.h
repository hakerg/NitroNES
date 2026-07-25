#pragma once
#include "NESHeadlessSystem.h"
#include "../src/core/Tracer.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

template <typename CoreT>
class TestRunner : public Tracer {
public:
    explicit TestRunner(const std::string& romPath)
        : nes(romPath) {
        nes.setTracer(this);
    }

    ~TestRunner() override {
        flushPending();
        closeTrace();
        nes.setTracer(nullptr);
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
                auto addr = (uint32_t)std::strtoul(hex.c_str(), nullptr, 16);
                if (addr <= 0xFFFF) symbolByAddr[(uint16_t)addr] = name;
                continue;
            }
            auto eq = line.find('=');
            if (eq == std::string::npos) continue;
            std::string name = line.substr(0, eq);
            std::string val  = line.substr(eq + 1);
            trim(name); trim(val);
            if (name.empty() || val.empty() || val[0] != '$') continue;
            auto addr = (uint32_t)std::strtoul(val.c_str() + 1, nullptr, 16);
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

    // Tracer ----------------------------------------------------------------
    void writeCpu(std::string_view body) override {
        flushPending();
        beginLine(body);
    }
    void writePpu(std::string_view body) override {
        flushPending();
        beginLine(body);
    }
    void appendDma(std::string_view body) override { appendToPending(body); }
    void appendApu(std::string_view body) override { appendToPending(body); }

    std::string symbolExact(uint16_t addr) const override {
        if (const char* io = nesIoRegName(addr)) return io;
        auto it = symbolByAddr.find(addr);
        return it == symbolByAddr.end() ? std::string{} : it->second;
    }

    std::string symbolNear(uint16_t pc) const override {
        if (symbolByAddr.empty()) return {};
        auto it = symbolByAddr.upper_bound(pc);
        if (it == symbolByAddr.begin()) return {};
        --it;
        uint16_t off = pc - it->first;
        if (off > 0x40) return {};
        if (off == 0) return it->second;
        return std::format("{}+{}", it->second, off);
    }

private:
    CoreT nes;
    std::map<uint16_t, std::string> symbolByAddr;

    std::FILE* traceFp = nullptr;
    std::string tracePath = "trace.log";
    std::string pending;

    // helpers ----------------------------------------------------------------
    void appendToPending(std::string_view body) {
        if (pending.empty()) return;
        pending += "  ";
        pending += body;
    }

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

    static std::vector<std::string> splitArgs(const std::string& cmd) {
        std::vector<std::string> out;
        std::string cur;
        for (char c : cmd) {
            if (c == ':') { out.push_back(cur); cur.clear(); }
            else cur.push_back(c);
        }
        out.push_back(cur);
        return out;
    }

    // dispatch ---------------------------------------------------------------
    bool dispatch(const std::string& cmd) {
        if (cmd == "reset") { nes.reset(); return true; }

        if (cmd.size() >= 5 && cmd.compare(0, 3, "pad") == 0
            && (cmd[3] == '1' || cmd[3] == '2'))
        {
            int  port = cmd[3] - '1';
            char op   = cmd[4];
            if (op != '=' && op != '+' && op != '-') return false;
            return padCmd(port, op, cmd.substr(5));
        }

        auto parts = splitArgs(cmd);
        const std::string& head = parts[0];

        if (head == "frames" && parts.size() == 2) {
            nes.tickFrames(parseInt(parts[1]));
            return true;
        }
        if (head == "cycles" && parts.size() == 2) {
            nes.tickCycles(parseInt(parts[1]));
            return true;
        }
        if (cmd == "info") {
            std::cout << "---- info frame=" << nes.frameNo() << " cyc=" << nes.cycleNo() << " ----\n";
            return true;
        }
        if (head == "mem" && parts.size() == 3) {
            printMem((uint16_t)parseInt(parts[1]), parseInt(parts[2]));
            return true;
        }
        if (head == "screen" && parts.size() == 1) {
            printScreen(false, {});
            return true;
        }
        if (head == "ascii") {
            auto colonPos = cmd.find(':');
            std::string charMap = (colonPos != std::string::npos)
                ? cmd.substr(colonPos + 1) : defaultAsciiMap();
            printScreen(true, charMap);
            return true;
        }
        if (head == "pixels" && parts.size() == 5) {
            printPixels((int)parseInt(parts[1]), (int)parseInt(parts[2]),
                        (int)parseInt(parts[3]), (int)parseInt(parts[4]));
            return true;
        }
        if (head == "song" && parts.size() == 2) {
            if constexpr (requires { nes.initSong(uint8_t{}); }) {
                nes.initSong((uint8_t)parseInt(parts[1]));
                return true;
            } else return false;
        }
        if (cmd == "next") {
            if constexpr (requires { nes.nextSong(); }) { nes.nextSong(); return true; }
            else return false;
        }
        if (cmd == "prev") {
            if constexpr (requires { nes.prevSong(); }) { nes.prevSong(); return true; }
            else return false;
        }
        if (cmd == "songinfo") {
            if constexpr (requires { nes.header(); }) { printSongInfo(); return true; }
            else return false;
        }
        if (head == "trace-file" && parts.size() == 2) {
            tracePath = parts[1];
            closeTrace();
            return true;
        }
        if (head == "trace" && parts.size() == 3) {
            bool on = (parts[2] == "on" || parts[2] == "1");
            if      (parts[1] == "cpu") cpu = on;
            else if (parts[1] == "ppu") ppu = on;
            else if (parts[1] == "dma") dma = on;
            else if (parts[1] == "apu") apu = on;
            else return false;
            if (any() && !traceFp) openTrace();
            return true;
        }
        if (head == "ac" && parts.size() == 3) {
            navigateAccuracyCoin((int)parseInt(parts[1]), (int)parseInt(parts[2]));
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
    static std::string defaultAsciiMap() {
        std::string map(256, ' ');
        for (int i = 0x20; i <= 0x7E; ++i)
            map[i] = (char)i;
        return map;
    }

    void printScreen(bool ascii, const std::string& charMap) {
        if constexpr (requires { nes.dumpNametable((uint8_t*)nullptr, 0); }) {
            uint8_t tiles[960]{};
            nes.dumpNametable(tiles, 0);
            std::cout << "---- screen (NT0) frame=" << nes.frameNo()
                      << " cyc=" << nes.cycleNo()
                      << (ascii ? " ascii" : " hex") << " ----\n";
            for (int row = 0; row < 30; ++row) {
                std::string line;
                for (int col = 0; col < 32; ++col) {
                    uint8_t t = tiles[row * 32 + col];
                    if (ascii) {
                        if (t < charMap.size()) line += charMap[t];
                        else line += ' ';
                    } else {
                        if (col) line += ' ';
                        line += std::format("{:02X}", t);
                    }
                }
                if (ascii) {
                    auto last = line.find_last_not_of(' ');
                    line = (last != std::string::npos) ? line.substr(0, last + 1) : "";
                }
                std::cout << line << "\n";
            }
        } else {
            std::cerr << "[nes_test] screen: not supported for this core\n";
        }
    }

    void printPixels(int x0, int y0, int w, int h) {
        if constexpr (requires { nes.framebuffer(); }) {
            const uint32_t* fb = nes.framebuffer();
            if (!fb) { std::cerr << "[nes_test] pixels: no framebuffer\n"; return; }
            std::cout << "---- pixels x=" << x0 << " y=" << y0
                      << " w=" << w << " h=" << h
                      << " frame=" << nes.frameNo() << " ----\n";
            for (int y = y0; y < y0 + h; ++y) {
                std::string line;
                for (int x = x0; x < x0 + w; ++x) {
                    if (y < 0 || y >= NES::SCREEN_HEIGHT
                        || x < 0 || x >= NES::SCREEN_WIDTH)
                        continue;
                    uint32_t rgb = fb[y * NES::SCREEN_WIDTH + x] & 0x00FFFFFFu;
                    if (!line.empty()) line += ' ';
                    line += std::format("{:06X}", rgb);
                }
                std::cout << line << "\n";
            }
        } else {
            std::cerr << "[nes_test] pixels: not supported for this core\n";
        }
    }

    void printSongInfo() {
        if constexpr (requires { nes.header(); }) {
            const auto& h = nes.header();
            std::cout << "---- NSF song " << (int)nes.getCurrentSong() << "/"
                      << (int)nes.getTotalSongs() << " \"" << h.title()
                      << "\" by " << h.artistName() << " (" << h.copyrightText() << ")"
                      << (h.isPAL() ? " [PAL]" : " [NTSC]") << " ----\n"
                      << "load=$" << hex4(h.loadAddr)
                      << " init=$" << hex4(h.initAddr)
                      << " play=$" << hex4(h.playAddr) << "\n";
        }
    }

    void printMem(uint16_t addr, uint32_t len) {
        std::cout << "---- mem $" << hex4(addr) << " len=" << len << " ----\n";
        for (uint32_t i = 0; i < len; i += 16) {
            std::string line = std::format("{:04X}: ", (uint16_t)(addr + i));
            for (uint32_t j = 0; j < 16 && i + j < len; ++j)
                line += std::format("{:02X} ", nes.peekMemory((uint16_t)(addr + i + j)));
            std::cout << line << "\n";
        }
    }

    // tracer plumbing --------------------------------------------------------
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

    void beginLine(std::string_view body) {
        if (!traceFp) openTrace();
        if (!traceFp) return;
        pending = std::format("F={} CYC={} ", nes.frameNo(), nes.cycleNo());
        pending += body;
    }

    void flushPending() {
        if (pending.empty() || !traceFp) { pending.clear(); return; }
        std::fputs(pending.c_str(), traceFp);
        std::fputc('\n', traceFp);
        pending.clear();
    }

    static std::string hex4(uint16_t v) {
        return std::format("{:04X}", v);
    }

    void navigateAccuracyCoin(int page, int row) {
        nes.tickFrames(60);

        for (int i = 1; i < page; ++i) {
            nes.setController1(0x01);
            nes.tickFrames(20);
            nes.setController1(0x00);
            nes.tickFrames(20);
        }

        for (int i = 0; i < row; ++i) {
            nes.setController1(0x04);
            nes.tickFrames(20);
            nes.setController1(0x00);
            nes.tickFrames(20);
        }

        nes.setController1(0x80);
    }
};
