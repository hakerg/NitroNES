#pragma once
#include "AudioSettings.h"
#include <cstdint>

static constexpr uint8_t LENGTH_TABLE[32] = {
    10, 254, 20,  2, 40,  4, 80,  6, 160,  8, 60, 10, 14, 12, 26, 14,
    12,  16, 24, 18, 48, 20, 96, 22, 192, 24, 72, 26, 16, 28, 32, 30
};

static constexpr uint8_t DUTY_TABLE[4][8] = {
    { 0, 1, 0, 0, 0, 0, 0, 0 },
    { 0, 1, 1, 0, 0, 0, 0, 0 },
    { 0, 1, 1, 1, 1, 0, 0, 0 },
    { 1, 0, 0, 1, 1, 1, 1, 1 },
};

static constexpr uint8_t TRIANGLE_TABLE[32] = {
    15, 14, 13, 12, 11, 10,  9,  8,  7,  6,  5,  4,  3,  2,  1,  0,
     0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15
};

static float PULSE_MIXER_TABLE[31];
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

template <bool IsPulse1>
struct PulseChannel {
    uint8_t  duty           = 0;
    bool     lengthHalt     = false;
    bool     constVolume    = false;
    uint8_t  volume         = 0;

    bool     sweepEnabled   = false;
    uint8_t  sweepPeriod    = 0;
    bool     sweepNegate    = false;
    uint8_t  sweepShift     = 0;

    uint16_t timerPeriod    = 0;
    uint8_t  lengthCounter  = 0;
    bool     enabled        = false;

    uint8_t  dutyPos        = 0;
    float    timerCounter   = 0;

    bool     envStart       = false;
    uint8_t  envDivider     = 0;
    uint8_t  envDecay       = 0;

    uint8_t  sweepDivider   = 0;
    bool     sweepReload    = false;

    AudioSettings& settings;

    PulseChannel(AudioSettings& settings) : settings(settings) {}

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
        if (enabled) lengthCounter = LENGTH_TABLE[data >> 3];
        if (!settings.reduceClicks) dutyPos = 0;
        envStart = true;
    }

    void clockTimer() {
        while (timerCounter <= 0) {
            timerCounter += timerPeriod + 1;
            dutyPos = (dutyPos + 1) & 0x07;
        }
        timerCounter -= settings.pitch;
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
        if (!lengthHalt && lengthCounter > 0)
            lengthCounter--;

        if (sweepDivider == 0) {
            uint16_t target = sweepTarget();
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

    uint16_t sweepTarget() const {
        auto delta = (int16_t)(timerPeriod >> sweepShift);
        if (sweepNegate)
            delta = IsPulse1 ? -(delta + 1) : -delta;
        int32_t target = (int32_t)timerPeriod + delta;
        return (uint16_t)(target < 0 ? 0 : target);
    }

    bool isMuted() const {
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

struct TriangleChannel {
    bool     lengthHalt        = false;
    uint8_t  linearCounterLoad = 0;
    uint16_t timerPeriod       = 0;
    uint8_t  lengthCounter     = 0;
    bool     enabled           = false;

    uint8_t  seqPos            = 0;
    float    timerCounter      = 0;
    uint8_t  linearCounter     = 0;
    bool     linearCounterReload = false;

    AudioSettings& settings;

    TriangleChannel(AudioSettings& settings) : settings(settings) {}

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
        while (timerCounter <= 0) {
            timerCounter += timerPeriod + 1;
            if (timerPeriod >= 2 && lengthCounter > 0 && linearCounter > 0)
                seqPos = (seqPos + 1) & 0x1F;
        }
        timerCounter -= settings.pitch;
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
        return TRIANGLE_TABLE[seqPos];
    }
};

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
    bool     modeFlag    = false;
    uint8_t  periodIndex = 0;
    uint8_t  lengthCounter = 0;
    bool     enabled       = false;

    float    timerCounter = 0;
    uint16_t shiftReg     = 1;

    bool     envStart    = false;
    uint8_t  envDivider  = 0;
    uint8_t  envDecay    = 0;

    AudioSettings& settings;

    NoiseChannel(AudioSettings& settings) : settings(settings) {}

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
        while (timerCounter <= 0) {
            const uint16_t period = NOISE_PERIOD_TABLE_NTSC[periodIndex];
            timerCounter += period + 1;
            uint16_t feedback = (shiftReg & 0x0001) ^
                                ((modeFlag ? (shiftReg >> 6) : (shiftReg >> 1)) & 0x0001);
            shiftReg >>= 1;
            shiftReg |= (feedback << 14);
        }
        timerCounter -= settings.pitch;
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
        if (shiftReg & 0x0001) return 0;
        return constVolume ? volume : envDecay;
    }
};

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

    uint8_t  outputLevel   = 0;
    uint16_t sampleAddr    = 0;
    uint16_t sampleLength  = 0;

    uint16_t timerCounter  = 0;
    uint16_t currentAddr   = 0;
    uint16_t bytesRemaining = 0;
    uint8_t  shiftReg      = 0;
    uint8_t  bitsRemaining = 8;
    uint8_t  sampleBuffer  = 0;
    bool     sampleBufferEmpty = true;
    bool     silenceFlag   = true;
    bool     irqPending    = false;
    uint8_t  dmaDelay      = 0;
    // Per nes_specs/dma.txt: a "load" DMC DMA (after $4015 D4 set with empty buffer)
    // attempts to halt the CPU on a get cycle (3 cycles), while a "reload" DMC DMA
    // (buffer emptied during playback) attempts to halt on a put cycle (4 cycles).
    // false => halt on get (load), true => halt on put (reload).
    bool     dmaHaltOnPut  = false;

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
        // A fetch caused directly by (re)starting playback is a "load" DMA: halt on get.
        dmaHaltOnPut  = false;
    }

    void tickDMADelay() {
        if (dmaDelay > 0) dmaDelay--;
    }

    bool needsDMAFetch() const {
        return sampleBufferEmpty && bytesRemaining > 0 && dmaDelay == 0;
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
            timerCounter = DMC_PERIOD_TABLE_NTSC[rateIndex];

            if (!silenceFlag) {
                if (shiftReg & 0x01) {
                    if (outputLevel < 126) outputLevel += 2;
                } else {
                    if (outputLevel > 1)   outputLevel -= 2;
                }
            }
            shiftReg >>= 1;
            bitsRemaining--;

            if (bitsRemaining == 0) {
                bitsRemaining = 8;
                if (sampleBufferEmpty) {
                    silenceFlag = true;
                } else {
                    silenceFlag       = false;
                    shiftReg          = sampleBuffer;
                    sampleBufferEmpty = true;
                    // Buffer emptied during playback => the next fetch is a "reload"
                    // DMC DMA, which attempts to halt the CPU on a put cycle.
                    dmaHaltOnPut      = true;
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

class APU {
public:
    APU(AudioSettings& settings) : pulse1(settings), pulse2(settings),
        triangle(settings), noise(settings) {
        initMixerTables();
        frameCounter = 0;
        frameMode = 0;
        frameIRQInhibit = false;
        frameIRQPending = false;
    }

    void reset() {
        pulse1.enabled  = false;  pulse1.lengthCounter  = 0;
        pulse2.enabled  = false;  pulse2.lengthCounter  = 0;
        triangle.enabled = false; triangle.lengthCounter = 0;
        noise.enabled   = false;  noise.lengthCounter   = 0;
        dmc.bytesRemaining   = 0;
        dmc.irqPending       = false;

        frameIRQPending = false;
        frame4015ClearPending = false;
        delay4017 = -1;
    }

    bool irqAsserted() const { return frameIRQLine || dmc.irqPending; }

    PulseChannel<true>  pulse1;
    PulseChannel<false> pulse2;
    TriangleChannel triangle;
    NoiseChannel    noise;
    DMCChannel      dmc;

    bool dmcNeedsSample() const { return dmc.needsDMAFetch(); }
    uint16_t dmcSampleAddress() const { return dmc.currentAddr; }
    bool dmcDMAHaltOnPut() const { return dmc.dmaHaltOnPut; }

    void loadDMCSample(uint8_t data) {
        if (dmc.bytesRemaining == 0) return;
        dmc.loadSampleBuffer(data);
    }

    bool frameIRQPending = false;
    bool frame4015ClearPending = false;
    bool frameIRQLine = false;
    bool dmcIRQPending() const { return dmc.irqPending; }

    void writeData(uint16_t addr, uint8_t data, bool isAPUPutCycle) {
        switch (addr) {
            case 0x4000: pulse1.writeR0(data); break;
            case 0x4001: pulse1.writeR1(data); break;
            case 0x4002: pulse1.writeR2(data); break;
            case 0x4003: pulse1.writeR3(data); break;
            case 0x4004: pulse2.writeR0(data); break;
            case 0x4005: pulse2.writeR1(data); break;
            case 0x4006: pulse2.writeR2(data); break;
            case 0x4007: pulse2.writeR3(data); break;
            case 0x4008: triangle.writeR0(data); break;
            case 0x400A: triangle.writeR2(data); break;
            case 0x400B: triangle.writeR3(data); break;
            case 0x400C: noise.writeR0(data); break;
            case 0x400E: noise.writeR2(data); break;
            case 0x400F: noise.writeR3(data); break;
            case 0x4010: dmc.writeR0(data); break;
            case 0x4011: dmc.writeR1(data); break;
            case 0x4012: dmc.writeR2(data); break;
            case 0x4013: dmc.writeR3(data); break;
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
                    // The load DMA after a $4015 enable is scheduled to halt on a get
                    // cycle during the 2nd APU cycle after the write (nes_specs/dma.txt),
                    // so its request is delayed a couple of CPU cycles.
                    dmc.dmaDelay = 3;
                }

                dmc.irqPending = false;
                break;
            case 0x4017:
                val4017 = data;
                frameIRQInhibit = (data >> 6) & 0x01;
                if (frameIRQInhibit) frameIRQPending = false;
                delay4017 = isAPUPutCycle ? 2 : 3;
                break;
            default:
                break;
            }
    }

    uint8_t readData(uint16_t addr, uint8_t openBus) {
        if (addr == 0x4015) {
            uint8_t status = openBus & 0x20;
            if (pulse1.lengthCounter   > 0) status |= 0x01;
            if (pulse2.lengthCounter   > 0) status |= 0x02;
            if (triangle.lengthCounter > 0) status |= 0x04;
            if (noise.lengthCounter    > 0) status |= 0x08;
            if (dmc.bytesRemaining     > 0) status |= 0x10;
            if (frameIRQPending)            status |= 0x40;
            if (dmc.irqPending)             status |= 0x80;
            frame4015ClearPending = true;
            return status;
        }
        return 0x00;
    }

    void clock(bool isAPUCycle) {
        if (!isAPUCycle && frame4015ClearPending) {
            frameIRQPending = false;
            frame4015ClearPending = false;
        }
        // The frame interrupt flag drives the CPU's IRQ line one CPU cycle after it
        // is set (the level detector is sampled the following cycle), so snapshot it
        // before the sequencer can update the flag this cycle.
        frameIRQLine = frameIRQPending && !frameIRQInhibit;
        dmc.tickDMADelay();
        frameCounter++;
        serviceFrameCounterWrite();
        triangle.clockTimer();
        if (isAPUCycle) clockChannelTimers();
        clockFrameSequencer();
    }

    float getOutputSample() const {
        uint8_t p1 = pulse1.output();
        uint8_t p2 = pulse2.output();
        uint8_t t  = triangle.output();
        uint8_t n  = noise.output();
        uint8_t d  = dmc.output();

        float pulseOut = PULSE_MIXER_TABLE[p1 + p2];
        float tndOut   = TND_MIXER_TABLE[3 * t + 2 * n + d];

        return pulseOut + tndOut;
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
        // TODO: PAL/DENDY
        const uint32_t step1 = false ? 8313  : 7457;
        const uint32_t step2 = false ? 16627 : 14913;
        const uint32_t step3 = false ? 24939 : 22371;
        const uint32_t step4 = false ? 33253 : 29829;
        const uint32_t step5 = false ? 41565 : 37281;

        if (frameMode == 0) {
            if (frameCounter == step1) { clockQuarterFrame(); }
            else if (frameCounter == step2) { clockQuarterFrame(); clockHalfFrame(); }
            else if (frameCounter == step3) { clockQuarterFrame(); }
            else if (frameCounter == step4 - 1) {
                frameIRQPending = true;
            }
            else if (frameCounter == step4) {
                clockQuarterFrame();
                clockHalfFrame();
                frameIRQPending = true;
            }
            else if (frameCounter == step4 + 1) {
                if (frameIRQInhibit) frameIRQPending = false;
                else                 frameIRQPending = true;
                frameCounter = 0;
            }
        }
        else {
            if      (frameCounter == step1) { clockQuarterFrame(); }
            else if (frameCounter == step2) { clockQuarterFrame(); clockHalfFrame(); }
            else if (frameCounter == step3) { clockQuarterFrame(); }
            // step4 (29829/33253): brak akcji
            else if (frameCounter == step5) { clockQuarterFrame(); clockHalfFrame(); }
            else if (frameCounter == step5 + 1) { frameCounter = 0; }
        }
    }

    uint32_t frameCounter   = 0;
    uint8_t  frameMode      = 0;
    bool     frameIRQInhibit = false;

    int delay4017 = -1;
    uint8_t val4017 = 0;

    void clockQuarterFrame() {
        pulse1.clockEnvelope();
        pulse2.clockEnvelope();
        noise.clockEnvelope();
        triangle.clockLinearCounter();
    }

    void clockHalfFrame() {
        pulse1.clockLengthAndSweep();
        pulse2.clockLengthAndSweep();
        triangle.clockLengthCounter();
        noise.clockLengthCounter();
    }
};