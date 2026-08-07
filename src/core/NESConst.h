#pragma once
#include <cstdint>

enum class NESStandard { NTSC, PAL, DENDY };

namespace NES {

    static constexpr double MASTER_CLOCK_NTSC = (315.0 / 88.0) * 6.0 * 1000000.0;
    static constexpr double MASTER_CLOCK_PAL = 4433618.75 * 6.0;
    static constexpr double CPU_CLOCK_NTSC = MASTER_CLOCK_NTSC / 12.0;
    static constexpr double CPU_CLOCK_PAL = MASTER_CLOCK_PAL / 16.0;
    static constexpr double CPU_CLOCK_DENDY = MASTER_CLOCK_PAL / 15.0;
    static constexpr double PPU_CLOCK_NTSC = MASTER_CLOCK_NTSC / 4.0;
    static constexpr double PPU_CLOCK_PAL = MASTER_CLOCK_PAL / 5.0;

    static constexpr uint16_t NSF_SPEED_NTSC = 0x411A;
    static constexpr uint16_t NSF_SPEED_PAL = 0x4E20;

    static constexpr int SCREEN_WIDTH = 256;
    static constexpr int SCREEN_HEIGHT = 240;

    static constexpr int SCANLINE_VISIBLE_FIRST = 0;
    static constexpr int SCANLINE_VISIBLE_LAST = 239;
    static constexpr int SCANLINE_VBLANK_START_NTSC = 241;
    static constexpr int SCANLINE_VBLANK_START_DENDY = 291;
    static constexpr int SCANLINE_PRERENDER_NTSC = 261;
    static constexpr int SCANLINE_PRERENDER_PAL = 311;
    static constexpr int TOTAL_SCANLINES_NTSC = 262;
    static constexpr int TOTAL_SCANLINES_PAL = 312;
    static constexpr int TOTAL_SCANLINES = TOTAL_SCANLINES_NTSC;
    static constexpr int PPU_CYCLES_PER_SCANLINE = 341;
    static constexpr int PPU_CYCLES_PER_FRAME_NTSC = TOTAL_SCANLINES_NTSC * PPU_CYCLES_PER_SCANLINE;
    static constexpr int PPU_CYCLES_PER_FRAME_PAL = TOTAL_SCANLINES_PAL * PPU_CYCLES_PER_SCANLINE;
    static constexpr int PPU_CYCLES_PER_FRAME = PPU_CYCLES_PER_FRAME_NTSC;

    static constexpr int PAR_NUM_NTSC = 8;
    static constexpr int PAR_DEN_NTSC = 7;
    static constexpr int PAR_NUM_PAL = 11;
    static constexpr int PAR_DEN_PAL = 8;
    static constexpr int PAR_NUM = PAR_NUM_NTSC;
    static constexpr int PAR_DEN = PAR_DEN_NTSC;

    // średnia przy wyłączonym/włączonym renderowaniu w PPU
    static constexpr double REFRESH_RATE_NTSC_OFF = PPU_CLOCK_NTSC / PPU_CYCLES_PER_FRAME_NTSC;
    static constexpr double REFRESH_RATE_NTSC_ON  = PPU_CLOCK_NTSC / (PPU_CYCLES_PER_FRAME_NTSC - 0.5);
    // PAL/Dendy: 2C07/UA6538 nie pomija cyklu w nieparzystej klatce
    static constexpr double REFRESH_RATE_PAL = PPU_CLOCK_PAL / PPU_CYCLES_PER_FRAME_PAL;

    static constexpr size_t PRG_BANK_SIZE = 16384;
    static constexpr size_t CHR_BANK_SIZE = 8192;
    static constexpr size_t MAX_PRG_BANKS = 512;
    static constexpr size_t MAX_CHR_BANKS = 256;
    static constexpr size_t MAX_PRG_ROM_SIZE = MAX_PRG_BANKS * PRG_BANK_SIZE;
    static constexpr size_t MAX_CHR_ROM_SIZE = MAX_CHR_BANKS * CHR_BANK_SIZE;
    static constexpr size_t MAX_PRG_RAM_SIZE = 256 * 1024;

}