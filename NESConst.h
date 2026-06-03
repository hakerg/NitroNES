#pragma once
#include <cstdint>
#include <cstdio>
#include <cstdarg>

// ============================================================
//  Stałe sprzętowe Famicom / NES
// ============================================================

namespace NES {

    // --- Zegar CPU ---
    static constexpr uint32_t CPU_CLOCK_NTSC     = 1789773;  // Hz (~21.477 MHz / 12)
    static constexpr uint32_t CPU_CLOCK_PAL      = 1662607;  // Hz (~26.601 MHz / 16)

    // --- Cykle CPU na klatkę PPU ---
    static constexpr uint32_t CPU_CYCLES_PER_FRAME_NTSC = 29780;
    static constexpr uint32_t CPU_CYCLES_PER_FRAME_PAL  = 33247;

    // --- Audio ---
    static constexpr float    AUDIO_VOLUME       = 2.5f;     // Mnożnik głośności wyjściowej
    static constexpr float    AUDIO_HP1_CUTOFF   = 90.0f;   // HP filtr 1 [Hz] - blokada DC (kondensator sprzęgający)
    static constexpr float    AUDIO_HP2_CUTOFF   = 440.0f;  // HP filtr 2 [Hz] - stopień wyjściowy NES
    static constexpr float    AUDIO_LP_CUTOFF    = 14000.0f; // LP filtr [Hz] - pasmo Famicoma

    // --- NSF ---
    // Standardowa prędkość odtwarzania NSF w 1/1000000 s (60 Hz NTSC)
    static constexpr uint16_t NSF_SPEED_NTSC     = 0x411A;   // ~16666 us = 60.0 Hz
    static constexpr uint16_t NSF_SPEED_PAL      = 0x4E20;   // 20000 us = 50.0 Hz

    // --- Przestrzeń adresowa ---
    static constexpr uint16_t APU_STATUS_ADDR    = 0x4015;
    static constexpr uint16_t APU_FRAME_CTR_ADDR = 0x4017;
    static constexpr uint16_t NSF_BANK_BASE      = 0x5FF8;   // Rejestr bankswitch NSF

    static constexpr int OVERSCAN_TOP = 8;
    static constexpr int OVERSCAN_BOTTOM = 8;
    static constexpr int VISIBLE_H = 240 - OVERSCAN_TOP - OVERSCAN_BOTTOM; // 224
    static constexpr int PAR_NUM = 8;  // NTSC PAR 8:7
    static constexpr int PAR_DEN = 7;

    // --- Timing ---
    // Maksymalne opóźnienie pętli NES/audio; zapobiega spiral-of-lag i przepełnieniu bufora.
    static constexpr double MAX_DELAY = 0.025; // seconds
    // Maksymalny mnożnik prędkości (TAB / RT); używany do prealokacji buforów audio.
    static constexpr double MAX_SPEED = 4.0;

    // --- Odswiezanie ---
    static constexpr float REFRESH_NTSC =
        (float)CPU_CLOCK_NTSC / (float)CPU_CYCLES_PER_FRAME_NTSC; // ~60.098 Hz
    static constexpr float REFRESH_PAL  =
        (float)CPU_CLOCK_PAL  / (float)CPU_CYCLES_PER_FRAME_PAL;  // ~50.007 Hz

    // --- Flagi diagnostyczne
    //  Nie wpływają na wydajność gdy są false (kompilator usuwa kod).
    // ---------------------------------------------------------------
    namespace Debug {
        // Loguj każdy zapis do rejestrów APU ($4000-$4017) wraz z cyklem CPU.
        // Format: [APU] cyc=XXXXXXXX  $AAAA <- 0xDD  (kanał)
        static constexpr bool LOG_APU_WRITES   = false;

        // Loguj kazdy cykl PPU i CPU do pliku cpu_trace.log. Po jednej linii
        // na koncu PPU::clock() i CPU::tick(). UWAGA: plik rosnie szybko,
        // wlaczac tylko do diagnostyki.
        static constexpr bool LOG_CYCLE_TRACE = false;

        // Ścieżka wspólnego pliku trace.
        static constexpr const char* CPU_TRACE_PATH = "cpu_trace.log";

        // Wspolny handle pliku trace dla CPU i PPU. Otwierany leniwie przy
        // pierwszym uzyciu tracef().
        inline std::FILE* traceFile() {
            static std::FILE* f = [] {
                std::FILE* p = std::fopen(CPU_TRACE_PATH, "w");
                if (p) std::setvbuf(p, nullptr, _IOFBF, 1 << 16);
                return p;
            }();
            return f;
        }
        inline void tracef(const char* fmt, ...) {
            std::FILE* f = traceFile();
            if (!f) return;
            va_list ap;
            va_start(ap, fmt);
            std::vfprintf(f, fmt, ap);
            va_end(ap);
        }

        // Włącz / wyłącz poszczególne filtry post-BlipBuffer.
        // Wyłączenie filtra może pomóc zlokalizować źródło artefaktów dźwiękowych.
        static constexpr bool FILTER_HP90_EN  = true; // High-pass 90 Hz  (blokada DC)
        static constexpr bool FILTER_HP440_EN = true; // High-pass 440 Hz (stopień wyjściowy)
        static constexpr bool FILTER_LP14K_EN = true; // Low-pass  14 kHz (pasmo Famicoma)
    }

} // namespace NES
