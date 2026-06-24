#pragma once

#include <cstdint>

// ============================================================================
// VRC6Audio - 3 kanaly expansion audio Konami VRCVI (2x pulse + sawtooth).
// ----------------------------------------------------------------------------
// Modul niezalezny od bankingu/IRQ mappera, dzieki czemu uzywany jest zarowno
// przez Mapper024/Mapper026 (gry .nes), jak i przez NSFPlayer (utwory NSF z
// flaga expansion=VRC6) bez wciagania nieuzywanej logiki mappera.
//
// Specyfikacja: nes_specs/vrc6 audio.txt + nes_specs/vrc6.txt (sekcja Sound).
//
// Mapowanie rejestrow (adresy "kanoniczne", jak dla VRC6a; dla VRC6b
// remap A0<->A1 zalatwia mapper przed wywolaniem writeReg()).
//   $9000  pulse1 ctrl   (GDDD VVVV)
//   $9001  pulse1 freq lo
//   $9002  pulse1 freq hi (X---FFFF)
//   $9003  sound ctrl     (---- -SHH : H halt, S freq shift)
//   $A000  pulse2 ctrl
//   $A001  pulse2 freq lo
//   $A002  pulse2 freq hi
//   $B000  sawtooth accum rate (--PPPPPP)
//   $B001  sawtooth freq lo
//   $B002  sawtooth freq hi
//
// Wyjscie: liniowa suma p1(0..15) + p2(0..15) + saw(0..31), znormalizowana
// do float w okolicy [0, 1). Skalowanie dobrane tak, by w grach Konami nie
// kasowac APU, ale byc wyraznie slyszalne (APU uzywa nieliniowego miksera
// dochodzacego do ~0.99).
// ============================================================================
class VRC6Audio {
public:
    VRC6Audio() { reset(); }

    void reset() {
        pulse_[0] = Pulse{};
        pulse_[1] = Pulse{};
        saw_ = Saw{};
        soundHalt_ = false;
        freqShift_ = 0;
        freqShiftCounter_ = 0;
    }

    // Zapis kanonicznego rejestru (po remapie A0<->A1 dla VRC6b).
    // Adresy spoza zakresu audio sa ignorowane.
    void writeReg(uint16_t reg, uint8_t data) {
        switch (reg & 0xF003) {
        case 0x9000:
            pulse_[0].writeCtrl(data);
            break;
        case 0x9001:
            pulse_[0].writeFreqLo(data);
            break;
        case 0x9002:
            pulse_[0].writeFreqHi(data);
            break;
        case 0x9003:
            writeSoundCtrl(data);
            break;

        case 0xA000:
            pulse_[1].writeCtrl(data);
            break;
        case 0xA001:
            pulse_[1].writeFreqLo(data);
            break;
        case 0xA002:
            pulse_[1].writeFreqHi(data);
            break;

        case 0xB000:
            saw_.writeAccum(data);
            break;
        case 0xB001:
            saw_.writeFreqLo(data);
            break;
        case 0xB002:
            saw_.writeFreqHi(data);
            break;

        default:
            break;
        }
    }

    // Pojedynczy takt CPU.
    void clock() {
        if (soundHalt_)
            return;
        // Frequency shift: dzieli efektywna predkosc taktowania kanalow
        // audio przez 1 / 16 / 256 (bity 1-2 z $9003).
        if (freqShift_ == 0) {
            tickChannels();
        } else {
            int mask = (freqShift_ == 1) ? 0x0F : 0xFF;
            if ((freqShiftCounter_++ & mask) == 0)
                tickChannels();
        }
    }

    // Aktualne wyjscie audio jako float ~[0, 1).
    float output() const {
        if (soundHalt_)
            return 0.0f;
        uint32_t sum = (uint32_t)pulse_[0].output() +
                       (uint32_t)pulse_[1].output() + (uint32_t)saw_.output();
        return (float)sum * (0.75f / 61.0f);
    }

private:
    void tickChannels() {
        pulse_[0].clock();
        pulse_[1].clock();
        saw_.clock();
    }

    void writeSoundCtrl(uint8_t d) {
        soundHalt_ = (d & 0x01) != 0;
        uint8_t f = (d >> 1) & 0x03;
        // 00 = brak, 01 = >>4 (16x wolniej), 10/11 = >>8 (256x wolniej)
        if (f == 0)
            freqShift_ = 0;
        else if (f == 1)
            freqShift_ = 1;
        else
            freqShift_ = 2;
        freqShiftCounter_ = 0;
    }

    // --- kanal pulse (4-bit) ---
    struct Pulse {
        uint8_t duty = 0;   // 0..7
        uint8_t volume = 0; // 0..15
        bool gate = false;  // 1 = tryb digitized
        uint16_t freq = 0;  // 12-bit
        bool enabled = false;
        uint16_t timer = 0;
        uint8_t step = 0; // 0..15

        void writeCtrl(uint8_t d) {
            volume = d & 0x0F;
            duty = (d >> 4) & 0x07;
            gate = (d & 0x80) != 0;
        }
        void writeFreqLo(uint8_t d) { freq = (uint16_t)((freq & 0x0F00) | d); }
        void writeFreqHi(uint8_t d) {
            freq = (uint16_t)((freq & 0x00FF) | ((d & 0x0F) << 8));
            bool en = (d & 0x80) != 0;
            if (!enabled && en) {
                step = 0;
                timer = freq;
            }
            enabled = en;
        }
        void clock() {
            if (!enabled)
                return;
            if (timer == 0) {
                timer = freq;
                step = (uint8_t)((step + 1) & 0x0F);
            } else {
                timer--;
            }
        }
        uint8_t output() const {
            if (!enabled)
                return 0;
            if (gate)
                return volume; // tryb digitized
            // step 0..15; duty (D+1)/16: aktywne gdy step <= D
            return (step <= duty) ? volume : (uint8_t)0;
        }
    };

    // --- kanal sawtooth (5-bit) ---
    struct Saw {
        uint8_t accumRate = 0; // 6-bit
        uint16_t freq = 0;     // 12-bit
        bool enabled = false;
        uint16_t timer = 0;
        uint8_t accum = 0; // 8-bit
        uint8_t step = 0;  // 0..13 (7 par)

        void writeAccum(uint8_t d) { accumRate = d & 0x3F; }
        void writeFreqLo(uint8_t d) { freq = (uint16_t)((freq & 0x0F00) | d); }
        void writeFreqHi(uint8_t d) {
            freq = (uint16_t)((freq & 0x00FF) | ((d & 0x0F) << 8));
            bool en = (d & 0x80) != 0;
            if (!enabled && en) {
                accum = 0;
                step = 0;
                timer = freq;
            }
            enabled = en;
        }
        void clock() {
            if (!enabled)
                return;
            if (timer == 0) {
                timer = freq;
                // Co drugi tick dodaje accumRate; po 14 tickach reset.
                step++;
                if ((step & 0x01) == 0)
                    accum = (uint8_t)(accum + accumRate);
                if (step >= 14) {
                    step = 0;
                    accum = 0;
                }
            } else
                timer--;
        }
        uint8_t output() const {
            if (!enabled)
                return 0;
            return (uint8_t)(accum >> 3); // 5-bit
        }
    };

    Pulse pulse_[2];
    Saw saw_;
    bool soundHalt_ = false;
    uint8_t freqShift_ = 0; // 0 = 1:1, 1 = 16:1, 2 = 256:1
    uint32_t freqShiftCounter_ = 0;
};
