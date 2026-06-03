#pragma once
#include <cmath>

// ============================================================
//  AudioFilter - filtr pierwszego rzędu RC (IIR)
//  Jeden szablon obsługuje tryb dolno- i górnoprzepustowy.
//
//  Filtr górnoprzepustowy (high-pass):
//      y[n] = alpha * (y[n-1] + x[n] - x[n-1])
//      alpha = RC / (RC + dt)  -- bliskie 1.0 dla niskich częstotliwości odcięcia
//
//  Filtr dolnoprzepustowy (low-pass):
//      y[n] = y[n-1] + alpha * (x[n] - y[n-1])
//      alpha = dt / (RC + dt)  -- bliskie 0.0 dla niskich częstotliwości odcięcia
//
//  Referencja: https://www.nesdev.org/wiki/APU_Mixer#Emulation
// ============================================================

enum class FilterType { HighPass, LowPass };

class AudioFilter {
public:
    AudioFilter() = default;

    // cutoffHz  - częstotliwość odcięcia w Hz
    // sampleRate - częstotliwość próbkowania w Hz
    AudioFilter(FilterType type, float cutoffHz, float sampleRate)
        : type(type)
    {
        const float dt = 1.0f / sampleRate;
        const float rc = 1.0f / (2.0f * 3.14159265f * cutoffHz);
        alpha = (type == FilterType::HighPass)
            ? rc / (rc + dt)
            : dt / (rc + dt);
    }

    // Przepuszcza pojedynczą próbkę przez filtr, zwraca przefiltrowaną wartość
    float process(float x) {
        float y;
        if (type == FilterType::HighPass) {
            y       = alpha * (prevY + x - prevX);
            prevX   = x;
            prevY   = y;
        } else {
            y       = prevY + alpha * (x - prevY);
            prevY   = y;
        }
        return y;
    }

    void reset() {
        prevX = 0.0f;
        prevY = 0.0f;
    }

private:
    FilterType type   = FilterType::LowPass;
    float      alpha  = 1.0f;
    float      prevX  = 0.0f;
    float      prevY  = 0.0f;
};
