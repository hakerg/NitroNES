#pragma once
#include <cstdint>
#include <string>

class Tracer {
public:
    virtual ~Tracer() = default;

    bool cpu = false, ppu = false, dma = false, apu = false;
    bool any() const { return cpu || ppu || dma || apu; }

    virtual void writeCpu(const char* body) = 0;
    virtual void writePpu(const char* body) = 0;
    virtual void appendDma(const char* body) = 0;
    virtual void appendApu(const char* body) = 0;

    virtual std::string symbolNear(uint16_t pc) const { (void)pc; return {}; }
    virtual std::string symbolExact(uint16_t addr) const { (void)addr; return {}; }
};

inline const char* nesIoRegName(uint16_t addr) {
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

