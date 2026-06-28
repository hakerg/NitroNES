#pragma once
#include <cstdint>

namespace NES {

    static constexpr double MASTER_CLOCK_NTSC = (315.0 / 88.0) * 6.0 * 1000000.0;
    static constexpr double MASTER_CLOCK_PAL = 4433618.75 * 6.0;
    static constexpr double CPU_CLOCK_NTSC = MASTER_CLOCK_NTSC / 12.0;
    static constexpr double CPU_CLOCK_PAL = MASTER_CLOCK_PAL / 16.0;
    static constexpr double PPU_CLOCK_NTSC = MASTER_CLOCK_NTSC / 4.0;
    static constexpr double PPU_CLOCK_PAL = MASTER_CLOCK_PAL / 5.0;

    static constexpr uint16_t NSF_SPEED_NTSC = 0x411A;
    static constexpr uint16_t NSF_SPEED_PAL = 0x4E20;

    static constexpr int SCREEN_WIDTH = 256;
    static constexpr int SCREEN_HEIGHT = 240;

    static constexpr int SCANLINE_VISIBLE_FIRST = 0;
    static constexpr int SCANLINE_VISIBLE_LAST = 239;
    static constexpr int SCANLINE_VBLANK_START = 241;
    static constexpr int SCANLINE_PRERENDER = 261;
    static constexpr int TOTAL_SCANLINES = 262;
    static constexpr int PPU_CYCLES_PER_SCANLINE = 341;
    static constexpr int PPU_CYCLES_PER_FRAME = TOTAL_SCANLINES * PPU_CYCLES_PER_SCANLINE;

    static constexpr int PAR_NUM = 8;
    static constexpr int PAR_DEN = 7;

    // średnia przy wyłączonym/włączonym renderowaniu w PPU
    static constexpr double REFRESH_RATE_NTSC_OFF = PPU_CLOCK_NTSC / PPU_CYCLES_PER_FRAME;
    static constexpr double REFRESH_RATE_NTSC_ON  = PPU_CLOCK_NTSC / (PPU_CYCLES_PER_FRAME - 0.5);

}