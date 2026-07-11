#pragma once
#include "../AudioSettings.h"
#include <array>
#include <cstdint>

class MMC5Audio {
public:
    void setSettings(AudioSettings& value) { settings = &value; }

    void reset() {
        pulse = {};
        pcm = 0;
        frameCounter = 0;
        timerDivider = false;
    }

    void write(uint16_t addr, uint8_t data) {
        if (addr >= 0x5000 && addr <= 0x5007) {
            Pulse& channel = pulse[(addr >> 2) & 1];
            switch (addr & 3) {
            case 0:
                channel.duty = data >> 6;
                channel.halt = (data & 0x20) != 0;
                channel.constant = (data & 0x10) != 0;
                channel.volume = data & 0x0F;
                break;
            case 2:
                channel.period = (channel.period & 0x0700) | data;
                break;
            case 3:
                channel.period = (channel.period & 0x00FF) | ((data & 7) << 8);
                if (channel.enabled) channel.length = lengthTable[data >> 3];
                channel.envelopeStart = true;
                channel.step = 0;
                break;
            }
            return;
        }
        if (addr == 0x5011) pcm = data;
        if (addr == 0x5015) {
            for (int i = 0; i < 2; i++) {
                pulse[i].enabled = (data & (1 << i)) != 0;
                if (!pulse[i].enabled) pulse[i].length = 0;
            }
        }
    }

    uint8_t status() const {
        return (pulse[0].length ? 1 : 0) | (pulse[1].length ? 2 : 0);
    }

    void clock() {
        timerDivider = !timerDivider;
        if (timerDivider) {
            const float pitch = settings ? settings->pitch : 1.0f;
            for (auto& channel : pulse) channel.clockTimer(pitch);
        }
        frameCounter++;
        if (frameCounter == 7457 || frameCounter == 22371) clockEnvelope();
        if (frameCounter == 14913 || frameCounter >= 29829) {
            clockEnvelope();
            clockLength();
            if (frameCounter >= 29829) frameCounter = 0;
        }
    }

    float output() const {
        const float pulseOut = (pulse[0].output() + pulse[1].output()) / 30.0f;
        const float pcmOut = pcm / 255.0f;
        return pulseOut * 0.25f + pcmOut * 0.25f;
    }

private:
    struct Pulse {
        uint8_t duty = 0;
        bool halt = false;
        bool constant = false;
        uint8_t volume = 0;
        uint16_t period = 0;
        bool enabled = false;
        uint8_t length = 0;
        uint8_t step = 0;
        float timer = 0;
        bool envelopeStart = false;
        uint8_t envelopeDivider = 0;
        uint8_t envelopeDecay = 0;

        void clockTimer(float pitch) {
            if (timer <= 0) {
                timer += period + 1;
                step = (step + 1) & 7;
            }
            timer -= pitch;
        }

        void clockEnvelope() {
            if (envelopeStart) {
                envelopeStart = false;
                envelopeDecay = 15;
                envelopeDivider = volume;
            } else if (envelopeDivider) {
                envelopeDivider--;
            } else {
                envelopeDivider = volume;
                if (envelopeDecay) envelopeDecay--;
                else if (halt) envelopeDecay = 15;
            }
        }

        uint8_t output() const {
            static constexpr uint8_t dutyTable[4][8] = {
                {0,1,0,0,0,0,0,0}, {0,1,1,0,0,0,0,0},
                {0,1,1,1,1,0,0,0}, {1,0,0,1,1,1,1,1}
            };
            if (!enabled || !length || period < 8 || !dutyTable[duty][step]) return 0;
            return constant ? volume : envelopeDecay;
        }
    };

    void clockEnvelope() {
        for (auto& channel : pulse) channel.clockEnvelope();
    }

    void clockLength() {
        for (auto& channel : pulse)
            if (channel.length && !channel.halt) channel.length--;
    }

    static constexpr std::array<uint8_t, 32> lengthTable = {
        10,254,20,2,40,4,80,6,160,8,60,10,14,12,26,14,
        12,16,24,18,48,20,96,22,192,24,72,26,16,28,32,30
    };

    std::array<Pulse, 2> pulse{};
    uint8_t pcm = 0;
    uint32_t frameCounter = 0;
    bool timerDivider = false;
    AudioSettings* settings = nullptr;
};
