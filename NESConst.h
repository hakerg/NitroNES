#pragma once
#include <cstdint>
#include <cstdio>
#include <cstdarg>

// ============================================================
//  Stałe sprzętowe Famicom / NES
// ============================================================

namespace NES {

    // --- Zegar ---
    static constexpr double MASTER_CLOCK_NTSC = (315.0 / 88.0) * 6.0 * 1000000.0;
    static constexpr double MASTER_CLOCK_PAL = 4433618.75 * 6.0;
    static constexpr double CPU_CLOCK_NTSC = MASTER_CLOCK_NTSC / 12.0;
    static constexpr double CPU_CLOCK_PAL = MASTER_CLOCK_PAL / 16.0;
    static constexpr double PPU_CLOCK_NTSC = MASTER_CLOCK_NTSC / 4.0;
    static constexpr double PPU_CLOCK_PAL = MASTER_CLOCK_PAL / 5.0;

    // --- Audio ---
    static constexpr float    AUDIO_VOLUME = 2.5f;
    static constexpr float    AUDIO_HP1_CUTOFF = 90.0f;
    static constexpr float    AUDIO_HP2_CUTOFF = 440.0f;
    static constexpr float    AUDIO_LP_CUTOFF = 14000.0f;

    // --- NSF ---
    // Standardowa prędkość odtwarzania NSF w 1/1000000 s (60 Hz NTSC)
    static constexpr uint16_t NSF_SPEED_NTSC = 0x411A;   // ~16666 us = 60.0 Hz
    static constexpr uint16_t NSF_SPEED_PAL = 0x4E20;   // 20000 us = 50.0 Hz

    // --- Przestrzeń adresowa ---
    static constexpr uint16_t APU_STATUS_ADDR = 0x4015;
    static constexpr uint16_t APU_FRAME_CTR_ADDR = 0x4017;
    static constexpr uint16_t NSF_BANK_BASE = 0x5FF8;   // Rejestr bankswitch NSF

    // --- Ekran PPU ---
    static constexpr int SCREEN_WIDTH = 256;
    static constexpr int SCREEN_HEIGHT = 240;

    // --- Skanline PPU ---
    static constexpr int SCANLINE_PRERENDER = -1;
    static constexpr int SCANLINE_VISIBLE_FIRST = 0;
    static constexpr int SCANLINE_VISIBLE_LAST = 239;
    static constexpr int SCANLINE_VBLANK_START = 241;
    static constexpr int SCANLINE_LAST = 261;
    static constexpr int TOTAL_SCANLINES = 262;
    static constexpr int PPU_CYCLES_PER_SCANLINE = 341;
    static constexpr int PPU_CYCLES_PER_FRAME = TOTAL_SCANLINES * PPU_CYCLES_PER_SCANLINE;

    static constexpr int OVERSCAN_TOP = 8;
    static constexpr int OVERSCAN_BOTTOM = 8;
    static constexpr int VISIBLE_H = SCREEN_HEIGHT - OVERSCAN_TOP - OVERSCAN_BOTTOM;
    static constexpr int PAR_NUM = 8;
    static constexpr int PAR_DEN = 7;

    static constexpr double REFRESH_RATE_NTSC_OFF = PPU_CLOCK_NTSC / PPU_CYCLES_PER_FRAME;
    static constexpr double REFRESH_RATE_NTSC_ON  = PPU_CLOCK_NTSC / (PPU_CYCLES_PER_FRAME - 0.5);

    // --- Timing ---
    static constexpr double MAX_LAG = 0.04; // s

} // namespace NES
