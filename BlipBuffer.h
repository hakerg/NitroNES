#pragma once
#include <cstdint>
#include <vector>
#include <cstring>
#include <cmath>

// ============================================================
//  BlipBuffer - bandlimitowany resampler sygnału APU
//
//  Każda zmiana (delta) amplitudy jest rozmywana przez jądro
//  Sinc-Blackman z sub-próbkową precyzją fazową.
//  Normalizacja sumy jądra per-fazę zapewnia stałą głośność.
//
//  Czas jest WZGLEDNY wzgledem biezacej bazy bufora (nie absolutny).
//  Caller (AudioStream) trzyma akumulator czasu i po kazdym flush
//  odejmuje od niego dokladny czas wypchnietych probek (count/sampleRate)
//  - dzieki temu wartosc relTime przekazywana tutaj nigdy nie rosnie
//  bez ograniczen, a precyzja double sie nie degraduje.
// ============================================================

class BlipBuffer {
public:
    static constexpr int KERNEL_SIZE = 16;
    static constexpr int KERNEL_HALF = KERNEL_SIZE / 2;
    static constexpr int PHASES      = 32;

    explicit BlipBuffer(int sampleRate)
        : sampleRate(sampleRate)
    {
        int cap = (int)std::ceil(sampleRate / 20.0) + KERNEL_SIZE * 4;
        accum.assign(cap, 0.0f);
        buildKernel();
    }

    // Zarejestruj wartość wyjścia APU w chwili relTime [s] liczonej
    // wzgledem biezacej bazy bufora (czyli od ostatniego flush).
    // delta liczona automatycznie względem poprzedniej wartości.
    void addSample(double relTime, float value) {
        float delta = value - prevOutput;
        if (delta == 0.0f) return;
        prevOutput = value;

        double pos  = relTime * sampleRate;
        int    ipos = (int)pos;
        double frac = pos - ipos;

        int phase0 = (int)(frac * PHASES);
        if (phase0 < 0)      phase0 = 0;
        if (phase0 > PHASES) phase0 = PHASES;
        int   phase1 = (phase0 < PHASES) ? phase0 + 1 : PHASES;
        float blend  = (float)(frac * PHASES - phase0);

        int limit = (int)accum.size();
        for (int i = 0; i < KERNEL_SIZE; i++) {
            int idx = ipos + i;
            if (idx >= 0 && idx < limit) {
                float k = kernel[phase0][i] * (1.0f - blend)
                        + kernel[phase1][i] *         blend;
                accum[idx] += k * delta;
            }
        }
    }

    // Ile próbek jest dostępnych do odczytu do chwili endRelTime [s]
    // (czas wzgledny od biezacej bazy bufora).
    int availableSamples(double endRelTime) const {
        double exact = endRelTime * sampleRate;
        int n = (int)exact;
        if (n > (int)accum.size()) n = (int)accum.size();
        return n > 0 ? n : 0;
    }

    // Odczytaj próbki do chwili endRelTime [s] (czas wzgledny).
    // Zwraca liczbę próbek. Bufor jest przesuwany - nastepne wywolania
    // addSample/availableSamples/readSamples powinny dostac czas
    // pomniejszony o (count / sampleRate) [s].
    int readSamples(float* out, int maxSamples, double endRelTime) {
        int count = availableSamples(endRelTime);
        if (count > maxSamples) count = maxSamples;
        if (count <= 0) return 0;

        for (int i = 0; i < count; i++) {
            runningSum += accum[i];
            out[i]      = runningSum;
        }

        int tail = (int)accum.size() - count;
        std::memmove(accum.data(), accum.data() + count, tail * sizeof(float));
        std::fill(accum.begin() + tail, accum.end(), 0.0f);

        return count;
    }

    int  getSampleRate() const { return sampleRate; }

    void reset() {
        std::fill(accum.begin(), accum.end(), 0.0f);
        prevOutput = 0.0f;
        runningSum = 0.0f;
    }

private:
    int    sampleRate;
    float  prevOutput = 0.0f;
    float  runningSum = 0.0f;

    std::vector<float> accum;
    float kernel[PHASES + 1][KERNEL_SIZE] = {};

    void buildKernel() {
        static constexpr float PI = 3.14159265358979f;

        for (int phase = 0; phase <= PHASES; phase++) {
            float sum = 0.0f;
            for (int i = 0; i < KERNEL_SIZE; i++) {
                // Sinc przesuwa się z fazą, okno Blackmana jest nieruchome po indeksie i
                float x = (float)(i - KERNEL_HALF + 1) - (float)phase / PHASES;
                float sinc;
                if (std::abs(x) < 1e-6f) {
                    sinc = 1.0f;
                } else {
                    sinc = std::sin(PI * x) / (PI * x);
                }

                // Okno Blackmana po stałej pozycji i (niezależne od fazy)
                float n  = (float)i;
                float N  = (float)(KERNEL_SIZE - 1);
                float window = 0.42f
                             - 0.5f  * std::cos(2.0f * PI * n / N)
                             + 0.08f * std::cos(4.0f * PI * n / N);

                kernel[phase][i] = sinc * window;
                sum += kernel[phase][i];
            }
            if (sum != 0.0f)
                for (int i = 0; i < KERNEL_SIZE; i++)
                    kernel[phase][i] /= sum;
        }
    }
};

