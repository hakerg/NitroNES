#pragma once
#include <cstdint>
#include <cmath>
#include <cstdio>

#include "NESConst.h"

// ============================================================
//  APU2A03 - Układ audio Ricoh 2A03 (wbudowany w CPU NES/FC)
//  Referencja: https://www.nesdev.org/wiki/APU
// ============================================================

// ------------------------------------------------------------
//  Tablice przeglądowe (zgodnie z dokumentacją NESDev)
// ------------------------------------------------------------

// Tablica długości nut (indeksowana przez 5-bitowy wpis z rejestru)
static constexpr uint8_t LENGTH_TABLE[32] = {
    10, 254, 20,  2, 40,  4, 80,  6, 160,  8, 60, 10, 14, 12, 26, 14,
    12,  16, 24, 18, 48, 20, 96, 22, 192, 24, 72, 26, 16, 28, 32, 30
};

// Sekwencje cyklu pracy (duty) dla kanałów Pulse
static constexpr uint8_t DUTY_TABLE[4][8] = {
    { 0, 1, 0, 0, 0, 0, 0, 0 }, // 12.5%
    { 0, 1, 1, 0, 0, 0, 0, 0 }, // 25%
    { 0, 1, 1, 1, 1, 0, 0, 0 }, // 50%
    { 1, 0, 0, 1, 1, 1, 1, 1 }, // 25% zanegowane
};

// Tablica trójkąta (32-stopniowa fala)
static constexpr uint8_t TRIANGLE_TABLE[32] = {
    15, 14, 13, 12, 11, 10,  9,  8,  7,  6,  5,  4,  3,  2,  1,  0,
     0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15
};

// Tablica szumów LFSR (tryb short: 93 kroki, normal: 32767 kroków)
// Nie jako tablica - generowane sprzętowo przez LFSR

// Nieliniowa tablica miksera Pulse (indeks: suma wyjść 0..30)
// square_table[n] = 95.52 / (8128.0 / n + 100)
static float PULSE_MIXER_TABLE[31];

// Nieliniowa tablica miksera TND (triangle/noise/dmc)
// tnd_table[n] = 163.67 / (24329.0 / n + 100)
static float TND_MIXER_TABLE[203];

static bool mixerTablesInitialized = false;
static void initMixerTables() {
    if (mixerTablesInitialized) return;
    PULSE_MIXER_TABLE[0] = 0.0f;
    for (int i = 1; i < 31; i++)
        PULSE_MIXER_TABLE[i] = 95.52f / (8128.0f / i + 100.0f);
    TND_MIXER_TABLE[0] = 0.0f;
    for (int i = 1; i < 203; i++)
        TND_MIXER_TABLE[i] = 163.67f / (24329.0f / i + 100.0f);
    mixerTablesInitialized = true;
}

// ============================================================
//  Kanał Pulse (1 i 2)
// ============================================================
struct PulseChannel {
    // Rejestry sprzętowe
    uint8_t  duty           = 0;   // Cykl pracy (2 bity)
    bool     lengthHalt     = false;
    bool     constVolume    = false;
    uint8_t  volume         = 0;   // Głośność lub okres obwiedni (4 bity)

    bool     sweepEnabled   = false;
    uint8_t  sweepPeriod    = 0;
    bool     sweepNegate    = false;
    uint8_t  sweepShift     = 0;

    uint16_t timerPeriod    = 0;   // 11-bitowy okres timera
    uint8_t  lengthCounter  = 0;
    bool     enabled        = false; // bit $4015 — gdy false: length wymuszony 0 i blokada R3

    // Stan wewnętrzny
    uint8_t  dutyPos        = 0;   // Pozycja sekwencera (0-7)
    uint16_t timerCounter   = 0;

    // Obwiednia (Envelope)
    bool     envStart       = false;
    uint8_t  envDivider     = 0;
    uint8_t  envDecay       = 0;

    // Sweep
    uint8_t  sweepDivider   = 0;
    bool     sweepReload    = false;

    // Różnica między Pulse1 i Pulse2 przy negacji (Pulse1 odejmuje + 1)
    bool     isPulse1       = false;

    void writeR0(uint8_t data) {
        duty        = (data >> 6) & 0x03;
        lengthHalt  = (data >> 5) & 0x01;
        constVolume = (data >> 4) & 0x01;
        volume      = data & 0x0F;
    }

    void writeR1(uint8_t data) {
        sweepEnabled = (data >> 7) & 0x01;
        sweepPeriod  = (data >> 4) & 0x07;
        sweepNegate  = (data >> 3) & 0x01;
        sweepShift   = data & 0x07;
        sweepReload  = true;
    }

    void writeR2(uint8_t data) {
        timerPeriod = (timerPeriod & 0x0700) | data;
    }

    void writeR3(uint8_t data) {
        timerPeriod = (timerPeriod & 0x00FF) | ((uint16_t)(data & 0x07) << 8);
        // NESDev: "If the enabled flag is set, the length counter is loaded
        // with the entry from the length table; otherwise it is unchanged."
        if (enabled) lengthCounter = LENGTH_TABLE[data >> 3];
        dutyPos  = 0;
        envStart = true;
    }

    void clockTimer() {
        if (timerCounter == 0) {
            timerCounter = timerPeriod;
            dutyPos = (dutyPos + 1) & 0x07;
        } else {
            timerCounter--;
        }
    }

    void clockEnvelope() {
        if (envStart) {
            envDecay   = 15;
            envDivider = volume;
            envStart   = false;
        } else {
            if (envDivider == 0) {
                envDivider = volume;
                if (envDecay > 0)
                    envDecay--;
                else if (lengthHalt)
                    envDecay = 15;
            } else {
                envDivider--;
            }
        }
    }

    void clockLengthAndSweep() {
        // Length counter
        if (!lengthHalt && lengthCounter > 0)
            lengthCounter--;

        // Sweep unit: divider jest taktowany NAJPIERW, potem sprawdzamy reload (wg apu_ref.txt)
        if (sweepDivider == 0) {
            uint16_t target = sweepTarget();
            // Aktualizuj okres tylko jeśli sweep jest włączony, shift > 0
            // i kanał nie jest wyciszony (timer >= 8 oraz target <= 0x7FF)
            if (sweepEnabled && sweepShift > 0 && !isMuted())
                timerPeriod = target;
        }

        if (sweepDivider == 0 || sweepReload) {
            sweepDivider = sweepPeriod;
            sweepReload  = false;
        } else {
            sweepDivider--;
        }
    }

    // Wynik shiftera sweepa (obliczany stale wg spec, niezależnie od włączenia sweepa)
    // NESDev: "If the target period is negative (overflow into the sign bit),
    // the target period is zeroed." — clampujemy do 0, żeby nie wywołać fałszywego
    // muting (0 nie jest > $7FF). Dotyczy zwłaszcza Pulse 1 z negate=1 i shift=0.
    uint16_t sweepTarget() const {
        int16_t delta = (int16_t)(timerPeriod >> sweepShift);
        if (sweepNegate)
            delta = isPulse1 ? -(delta + 1) : -delta;
        int32_t target = (int32_t)timerPeriod + delta;
        return (uint16_t)(target < 0 ? 0 : target);
    }

    bool isMuted() const {
        // apu_ref.txt: "When the channel's period is less than 8 or the result
        // of the shifter is greater than $7FF, the channel's DAC receives 0"
        if (timerPeriod < 8) return true;
        if (sweepTarget() > 0x7FF) return true;
        return false;
    }

    uint8_t output() const {
        if (lengthCounter == 0) return 0;
        if (DUTY_TABLE[duty][dutyPos] == 0) return 0;
        if (isMuted()) return 0;
        return constVolume ? volume : envDecay;
    }
};

// ============================================================
//  Kanał Triangle
// ============================================================
struct TriangleChannel {
    bool     lengthHalt        = false;  // Podwójna rola: Control + halt
    uint8_t  linearCounterLoad = 0;
    uint16_t timerPeriod       = 0;
    uint8_t  lengthCounter     = 0;
    bool     enabled           = false;

    uint8_t  seqPos            = 0;
    uint16_t timerCounter      = 0;
    uint8_t  linearCounter     = 0;
    bool     linearCounterReload = false;

    void writeR0(uint8_t data) {
        lengthHalt        = (data >> 7) & 0x01;
        linearCounterLoad = data & 0x7F;
    }

    void writeR2(uint8_t data) {
        timerPeriod = (timerPeriod & 0x0700) | data;
    }

    void writeR3(uint8_t data) {
        timerPeriod          = (timerPeriod & 0x00FF) | ((uint16_t)(data & 0x07) << 8);
        if (enabled) lengthCounter = LENGTH_TABLE[data >> 3];
        linearCounterReload  = true;
    }

    void clockTimer() {
        if (timerCounter == 0) {
            timerCounter = timerPeriod;
            // Sprzętowo: seqPos nie jest taktowany przy timerPeriod < 2 (ultrasonik).
            // Zamrożenie seqPos zapobiega skokowi DC przy starcie kolejnej nuty.
            if (timerPeriod >= 2 && lengthCounter > 0 && linearCounter > 0)
                seqPos = (seqPos + 1) & 0x1F;
        } else {
            timerCounter--;
        }
    }

    void clockLinearCounter() {
        if (linearCounterReload)
            linearCounter = linearCounterLoad;
        else if (linearCounter > 0)
            linearCounter--;

        if (!lengthHalt)
            linearCounterReload = false;
    }

    void clockLengthCounter() {
        if (!lengthHalt && lengthCounter > 0)
            lengthCounter--;
    }

    uint8_t output() const {
        // DAC zawsze wystawia aktualną wartość sekwencera.
        // Cisza = seqPos zamrożony przez clockTimer(), nie skok do zera.
        return TRIANGLE_TABLE[seqPos];
    }
};

// ============================================================
//  Kanał Noise
// ============================================================

// Okresy timera szumu (NTSC).
// UWAGA: specyfikacja NESdev podaje wartości w cyklach CPU
// (4, 8, 16, ..., 4068), ale ten kanał taktujemy w rytmie APU
// (clockTimer() wywoływany co drugi cykl CPU), więc wartości muszą
// być w cyklach APU — czyli z tabeli specu / 2.
static constexpr uint16_t NOISE_PERIOD_TABLE_NTSC[16] = {
       1,    3,    7,   15,   31,   47,   63,   79,
     100,  126,  189,  253,  380,  507, 1016, 2033
};

static constexpr uint16_t NOISE_PERIOD_TABLE_PAL[16] = {
       1,    3,    6,   14,   29,   43,   58,   73,
      93,  117,  176,  235,  353,  471,  944, 1888
};

struct NoiseChannel {
    bool     lengthHalt  = false;
    bool     constVolume = false;
    uint8_t  volume      = 0;
    bool     modeFlag    = false; // Bit 7 ($400E): tryb short (93) vs long (32767)
    uint8_t  periodIndex = 0;
    uint8_t  lengthCounter = 0;
    bool     enabled       = false;

    uint16_t timerCounter = 0;
    uint16_t shiftReg     = 1; // LFSR - startuje od 1 per spec

    bool     envStart    = false;
    uint8_t  envDivider  = 0;
    uint8_t  envDecay    = 0;

    // Wskaźnik na bieżącą tabelę okresów (NTSC / PAL). Ustawiany przez APU.
    const uint16_t* periodTable = NOISE_PERIOD_TABLE_NTSC;

    void writeR0(uint8_t data) {
        lengthHalt  = (data >> 5) & 0x01;
        constVolume = (data >> 4) & 0x01;
        volume      = data & 0x0F;
    }

    void writeR2(uint8_t data) {
        modeFlag    = (data >> 7) & 0x01;
        periodIndex = data & 0x0F;
    }

    void writeR3(uint8_t data) {
        if (enabled) lengthCounter = LENGTH_TABLE[data >> 3];
        envStart = true;
    }

    void clockTimer() {
        if (timerCounter == 0) {
            timerCounter = periodTable[periodIndex];

            // LFSR: feedback z bitu 0 i bitu 6 (mode=1) lub bitu 1 (mode=0)
            uint16_t feedback = (shiftReg & 0x0001) ^
                                ((modeFlag ? (shiftReg >> 6) : (shiftReg >> 1)) & 0x0001);
            shiftReg >>= 1;
            shiftReg |= (feedback << 14);
        } else {
            timerCounter--;
        }
    }

    void clockEnvelope() {
        if (envStart) {
            envDecay   = 15;
            envDivider = volume;
            envStart   = false;
        } else {
            if (envDivider == 0) {
                envDivider = volume;
                if (envDecay > 0)
                    envDecay--;
                else if (lengthHalt)
                    envDecay = 15;
            } else {
                envDivider--;
            }
        }
    }

    void clockLengthCounter() {
        if (!lengthHalt && lengthCounter > 0)
            lengthCounter--;
    }

    uint8_t output() const {
        if (lengthCounter == 0) return 0;
        if (shiftReg & 0x0001) return 0; // Bit 0 LFSR = 1 -> cisza
        return constVolume ? volume : envDecay;
    }
};

// ============================================================
//  Kanał DMC (Delta Modulation Channel)
// ============================================================

// Okresy timera DMC (NTSC).
// UWAGA: spec NESdev podaje wartości w cyklach CPU (428, 380, ..., 54),
// ale clockTimer() wywoływany jest w rytmie APU (co drugi cykl CPU),
// więc wartości muszą być w cyklach APU – z tabeli specu / 2.
// "A rate of 428 means the output level changes every 214 APU cycles."
static constexpr uint16_t DMC_PERIOD_TABLE_NTSC[16] = {
    213, 189, 169, 159, 142, 126, 112, 106,
     94,  79,  70,  63,  52,  41,  35,  26
};

static constexpr uint16_t DMC_PERIOD_TABLE_PAL[16] = {
    198, 176, 157, 148, 137, 117, 104,  98,
     87,  73,  65,  58,  48,  38,  32,  24
};

struct DMCChannel {
    bool     irqEnabled    = false;
    bool     loopFlag      = false;
    uint8_t  rateIndex     = 0;

    uint8_t  outputLevel   = 0;  // 7-bitowy DAC
    uint16_t sampleAddr    = 0;
    uint16_t sampleLength  = 0;

    // Stan wewnętrzny
    uint16_t timerCounter  = 0;
    uint16_t currentAddr   = 0;
    uint16_t bytesRemaining = 0;
    uint8_t  shiftReg      = 0;
    uint8_t  bitsRemaining = 8;  // Na starcie liczymy od 8 (wg apu_ref.txt: "counter is loaded with 8")
    uint8_t  sampleBuffer  = 0;
    bool     sampleBufferEmpty = true;
    bool     silenceFlag   = true;
    bool     irqPending    = false;

    // Wskaźnik na bieżącą tabelę okresów (NTSC / PAL). Ustawiany przez APU.
    const uint16_t* periodTable = DMC_PERIOD_TABLE_NTSC;

    void writeR0(uint8_t data) {
        irqEnabled = (data >> 7) & 0x01;
        loopFlag   = (data >> 6) & 0x01;
        rateIndex  = data & 0x0F;
        if (!irqEnabled) irqPending = false;
    }

    void writeR1(uint8_t data) {
        outputLevel = data & 0x7F;
    }

    void writeR2(uint8_t data) {
        sampleAddr = 0xC000 | ((uint16_t)data << 6);
    }

    void writeR3(uint8_t data) {
        sampleLength = ((uint16_t)data << 4) | 0x0001;
    }

    void restart() {
        currentAddr   = sampleAddr;
        bytesRemaining = sampleLength;
    }

    // Wywoływane przez APU gdy potrzebuje załadować bajt próbki z pamięci.
    // Zwraca adres, który należy odczytać z CPU Bus (DMA).
    bool needsDMAFetch() const {
        return sampleBufferEmpty && bytesRemaining > 0;
    }

    void loadSampleBuffer(uint8_t data) {
        sampleBuffer      = data;
        sampleBufferEmpty = false;
        currentAddr = ((currentAddr + 1) & 0xFFFF) | 0x8000;
        bytesRemaining--;
        if (bytesRemaining == 0) {
            if (loopFlag)
                restart();
            else if (irqEnabled)
                irqPending = true;
        }
    }

    void clockTimer() {
        if (timerCounter == 0) {
            timerCounter = periodTable[rateIndex];

            // Output unit: jeden krok sekwencera (wg apu_ref.txt)
            if (!silenceFlag) {
                if (shiftReg & 0x01) {
                    if (outputLevel < 126) outputLevel += 2;  // counter < 126
                } else {
                    if (outputLevel > 1)   outputLevel -= 2;  // counter > 1
                }
            }
            shiftReg >>= 1;
            bitsRemaining--;

            if (bitsRemaining == 0) {
                bitsRemaining = 8;
                if (sampleBufferEmpty) {
                    silenceFlag = true;
                } else {
                    silenceFlag   = false;
                    shiftReg      = sampleBuffer;
                    sampleBufferEmpty = true;
                }
            }
        } else {
            timerCounter--;
        }
    }

    uint8_t output() const {
        return outputLevel;
    }
};

// ============================================================
//  APU2A03 - główna klasa
// ============================================================
class APU2A03 {
public:
    APU2A03() {
        initMixerTables();
        frameCounter  = 0;
        frameMode     = 0;
        frameIRQInhibit = false;
        frameIRQPending = false;
        pulse1.isPulse1 = true;
        pulse2.isPulse1 = false;
        setPAL(false);
    }

    void reset() {
        pulse1.enabled  = false;  pulse1.lengthCounter  = 0;
        pulse2.enabled  = false;  pulse2.lengthCounter  = 0;
        triangle.enabled = false; triangle.lengthCounter = 0;
        noise.enabled   = false;  noise.lengthCounter   = 0;
        dmc.bytesRemaining   = 0;
        dmc.irqPending       = false;

        frameIRQPending = false;
        frameIRQReadClear = false;

        cpuWrite(0x4017, val4017);
    }

    // Przełącza tryb taktowania kanałów (tabele okresów + progi frame countera).
    // Wywołaj raz przy ładowaniu ROM-u / NSF (przed `clock()`).
    void setPAL(bool enable) {
        palMode = enable;
        noise.periodTable = palMode ? NOISE_PERIOD_TABLE_PAL : NOISE_PERIOD_TABLE_NTSC;
        dmc.periodTable = palMode ? DMC_PERIOD_TABLE_PAL : DMC_PERIOD_TABLE_NTSC;

        // Progi frame countera Mierzone w cyklach CPU!
        if (palMode) {
            fcStep[0] = 8313;  fcStep[1] = 16627;
            fcStep[2] = 24939; fcStep[3] = 33253;
            fcStep5End = 41565;
        }
        else {
            fcStep[0] = 7457;  fcStep[1] = 14913;
            fcStep[2] = 22371; fcStep[3] = 29829;
            fcStep5End = 37281;
        }
    }
    bool isPAL() const { return palMode; }

    // Stan linii IRQ APU (frame counter + DMC). Sprzętowo to OR obu źródeł.
    bool irqAsserted() const { return frameIRQPending || dmc.irqPending; }

    PulseChannel    pulse1;
    PulseChannel    pulse2;
    TriangleChannel triangle;
    NoiseChannel    noise;
    DMCChannel      dmc;

    // Czy DMC potrzebuje bajtu z pamięci (CPU powinno sprawdzić i dostarczyć przez loadDMCSample)
    bool  dmcNeedsSample() const { return dmc.needsDMAFetch(); }
    uint16_t dmcSampleAddress() const { return dmc.currentAddr; }
    void  loadDMCSample(uint8_t data) { dmc.loadSampleBuffer(data); }

    bool  frameIRQPending  = false;
    bool  dmcIRQPending()  const { return dmc.irqPending; }

    // --- Zapis przez CPU (adresy 0x4000 - 0x4017) ---
    void cpuWrite(uint16_t addr, uint8_t data) {
        switch (addr) {
            // Pulse 1
            case 0x4000: pulse1.writeR0(data); break;
            case 0x4001: pulse1.writeR1(data); break;
            case 0x4002: pulse1.writeR2(data); break;
            case 0x4003: pulse1.writeR3(data); break;
            // Pulse 2
            case 0x4004: pulse2.writeR0(data); break;
            case 0x4005: pulse2.writeR1(data); break;
            case 0x4006: pulse2.writeR2(data); break;
            case 0x4007: pulse2.writeR3(data); break;
            // Triangle
            case 0x4008: triangle.writeR0(data); break;
            case 0x400A: triangle.writeR2(data); break;
            case 0x400B: triangle.writeR3(data); break;
            // Noise
            case 0x400C: noise.writeR0(data); break;
            case 0x400E: noise.writeR2(data); break;
            case 0x400F: noise.writeR3(data); break;
            // DMC
            case 0x4010: dmc.writeR0(data); break;
            case 0x4011: dmc.writeR1(data); break;
            case 0x4012: dmc.writeR2(data); break;
            case 0x4013: dmc.writeR3(data); break;
            // Status ($4015)
            case 0x4015:
                pulse1.enabled   = (data & 0x01) != 0;
                pulse2.enabled   = (data & 0x02) != 0;
                triangle.enabled = (data & 0x04) != 0;
                noise.enabled    = (data & 0x08) != 0;
                if (!pulse1.enabled)   pulse1.lengthCounter   = 0;
                if (!pulse2.enabled)   pulse2.lengthCounter   = 0;
                if (!triangle.enabled) triangle.lengthCounter = 0;
                if (!noise.enabled)    noise.lengthCounter    = 0;
                if (!(data & 0x10)) {
                    dmc.bytesRemaining = 0;
                } else if (dmc.bytesRemaining == 0) {
                    dmc.restart();
                }
                dmc.irqPending = false;
                break;
            // Frame Counter ($4017)
            case 0x4017:
                val4017 = data;
                frameIRQInhibit = (data >> 6) & 0x01;
                if (frameIRQInhibit) frameIRQPending = false;

                {
                    bool isAPUCycle = !apuGetPhase;
                    delay4017 = isAPUCycle ? 2 : 3;
                }
                break;
        }
    }

    // --- Odczyt przez CPU ($4015 Status) ---
    uint8_t cpuRead(uint16_t addr, uint8_t openBus) {
        if (addr == 0x4015) {
            uint8_t status = openBus & 0x20; // bit 5 — open bus (nienapędzany przez APU)
            if (pulse1.lengthCounter   > 0) status |= 0x01;
            if (pulse2.lengthCounter   > 0) status |= 0x02;
            if (triangle.lengthCounter > 0) status |= 0x04;
            if (noise.lengthCounter    > 0) status |= 0x08;
            if (dmc.bytesRemaining     > 0) status |= 0x10;
            if (frameIRQPending)            status |= 0x40;
            if (dmc.irqPending)             status |= 0x80;
            if (apuGetPhase) frameIRQPending  = false;
            else             frameIRQReadClear = true;
            return status;
        }
        return 0x00;
    }

    void clock(bool getCycle) {
        apuGetPhase = getCycle;

        if (apuGetPhase && frameIRQReadClear) {
            frameIRQPending   = false;
            frameIRQReadClear = false;
        }

        frameCounter++;

        serviceFrameCounterWrite();

        triangle.clockTimer();

        if (apuGetPhase) clockChannelTimers();

        clockFrameSequencer();
    }

private:
    void clockChannelTimers() {
        pulse1.clockTimer();
        pulse2.clockTimer();
        noise.clockTimer();
        dmc.clockTimer();
    }

    void serviceFrameCounterWrite() {
        if (delay4017 < 0) return;
        if (delay4017 == 0) {
            frameMode = (val4017 >> 7) & 0x01;
            frameCounter = 0;
            if (frameMode == 1) {
                clockQuarterFrame();
                clockHalfFrame();
            }
        }
        delay4017--;
    }

    void clockFrameSequencer() {
        if (frameMode == 0) {
            // Tryb 4-step
            if (frameCounter == fcStep[0]) { clockQuarterFrame(); }
            else if (frameCounter == fcStep[1]) { clockQuarterFrame(); clockHalfFrame(); }
            else if (frameCounter == fcStep[2]) { clockQuarterFrame(); }
            else if (frameCounter == fcStep[3] - 1) {
                if (!frameIRQInhibit) frameIRQPending = true;
            }
            else if (frameCounter == fcStep[3]) {
                clockQuarterFrame();
                clockHalfFrame();
                if (!frameIRQInhibit) frameIRQPending = true;
            }
            else if (frameCounter == fcStep[3] + 1) {
                if (!frameIRQInhibit) frameIRQPending = true;
                frameCounter = 0;
            }
        }
        else {
            // Tryb 5-step (brak IRQ z sekwencera)
            if (frameCounter == fcStep[0]) { clockQuarterFrame(); }
            else if (frameCounter == fcStep[1]) { clockQuarterFrame(); clockHalfFrame(); }
            else if (frameCounter == fcStep[2]) { clockQuarterFrame(); }
            else if (frameCounter == fcStep5End) {
                clockQuarterFrame();
                clockHalfFrame();
            }
            else if (frameCounter == fcStep5End + 1) {
                frameCounter = 0;
            }
        }
    }

public:

    // --- Wyjście miksera (wywołaj po clock(), aby pobrać próbkę audio bieżącego cyklu) ---
    // Zwraca wartość z zakresu [0.0, 1.0]
    float getOutputSample() const {
        uint8_t p1 = pulse1.output();
        uint8_t p2 = pulse2.output();
        uint8_t t  = triangle.output();
        uint8_t n  = noise.output();
        uint8_t d  = dmc.output();

        // Nieliniowy mikser zgodny ze sprzętowym DAC Ricoh 2A03
        float pulseOut = PULSE_MIXER_TABLE[p1 + p2];
        float tndOut   = TND_MIXER_TABLE[3 * t + 2 * n + d];

        return pulseOut + tndOut;
    }

private:
    bool     apuGetPhase    = true;
    bool     frameIRQReadClear = false;
    uint32_t frameCounter   = 0;
    uint8_t  frameMode      = 0;   // 0 = 4-step, 1 = 5-step
    bool     frameIRQInhibit = false;

    // Region timing
    bool     palMode        = false;
    uint32_t fcStep[4]      = { 7457, 14913, 22371, 29829 }; // NTSC 4-step, nadpisywane przez setPAL()
    uint32_t fcStep5End     = 37281;                          // NTSC 5-step end

    int delay4017 = -1;
    uint8_t val4017 = 0;

    // Quarter-frame: obwiednie i licznik liniowy trójkąta
    void clockQuarterFrame() {
        pulse1.clockEnvelope();
        pulse2.clockEnvelope();
        noise.clockEnvelope();
        triangle.clockLinearCounter();
    }

    // Half-frame: length countery i sweep units
    void clockHalfFrame() {
        pulse1.clockLengthAndSweep();
        pulse2.clockLengthAndSweep();
        triangle.clockLengthCounter();
        noise.clockLengthCounter();
    }
};
