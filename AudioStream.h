#pragma once
#include <vector>
#include <cmath>
#include <cstring>

#include "NESConst.h"
#include "AudioFilter.h"

// ============================================================
//  AudioStream - bandlimitowany resampler sygnalu APU (Sinc-Blackman)
//  z filtrami NES i interfejsem dla klas pochodnych (backendy audio).
//
//  Watek NES wola:
//    addSample(virtualDt, value)  - resampluje probke bezposrednio
//    commitBatch()                - filtruje i wywoluje
//                                   submitSamples() z gotowymi probkami
//
//  Klasy pochodne implementuja submitSamples() (np. SDLAudioStream).
// ============================================================

class AudioStream {
public:
	static constexpr int CHANNELS    = 1;
	static constexpr int KERNEL_SIZE = 16;
	static constexpr int KERNEL_HALF = KERNEL_SIZE / 2;
	static constexpr int PHASES      = 128;

	AudioStream() = default;
	virtual ~AudioStream() = default;

	// Inicjalizuje resampler i filtry dla podanego sampleRate.
	// Powinna byc wywolana przez klase pochodna po ustaleniu sampleRate.
	bool init(int rate) {
		sampleRate = rate;

		blipPrevOutput = 0.0f;
		blipRunningSum = 0.0f;
		buildKernel();

		hpf90  = AudioFilter(FilterType::HighPass, NES::AUDIO_HP1_CUTOFF, (float)sampleRate);
		hpf440 = AudioFilter(FilterType::HighPass, NES::AUDIO_HP2_CUTOFF, (float)sampleRate);
		lpf14k = AudioFilter(FilterType::LowPass,  NES::AUDIO_LP_CUTOFF,  (float)sampleRate);

		// Prealokacja buforów: MAX_DELAY * MAX_SPEED pokrywa najgorszy przypadek
		// (4x przyspieszenie + pelny 20ms batch) bez realokacji w hot path.
		const int maxSamples = (int)std::ceil(
			NES::MAX_DELAY * NES::MAX_SPEED * sampleRate) + 16;
		outBuf.reserve(maxSamples);

		// blipAccum musi pomiescic caly batch (maxSamples) plus ogon jadra
		// Sinc (KERNEL_SIZE probek). Mniejszy rozmiar powodowalby ciche
		// odrzucanie ogona jadra przy probkach blisko konca batcha, co
		// zaburza bilans delt i generuje klikniecia na granicy batchow.
		blipAccum.resize(maxSamples + KERNEL_SIZE, 0.0f);

		timeAccum    = 0.0;
		batchTimeAcc = 0.0;

		return true;
	}

	// Wola z watku NES - resampluje probke bezposrednio.
	void addSample(double virtualDt, float value) {
		batchTimeAcc += virtualDt;
		blipAddSample(timeAccum + batchTimeAcc, value);
	}

	// Wola z watku NES raz na batch - filtruje
	// i przekazuje probki do submitSamples().
	void commitBatch() {
		if (blipAccum.empty()) return;

		timeAccum += batchTimeAcc;
		batchTimeAcc = 0.0;

		// Odczytaj wszystkie dostepne probki
		int avail = blipAvailable(timeAccum);
		if (avail <= 0) return;

		outBuf.resize(avail); // nie alokuje - pojemnosc zagwarantowana w init
		int got = blipRead(outBuf.data(), avail, timeAccum);
		if (got <= 0) return;

		timeAccum -= (double)got / sampleRate;

		// Filtry NES + normalizacja glosnosci
		for (int i = 0; i < got; i++) {
			float y = outBuf[i];
			if constexpr (NES::Debug::FILTER_HP90_EN)  y = hpf90.process(y);
			if constexpr (NES::Debug::FILTER_HP440_EN) y = hpf440.process(y);
			if constexpr (NES::Debug::FILTER_LP14K_EN) y = lpf14k.process(y);
			y *= NES::AUDIO_VOLUME;
			outBuf[i] = y;
		}
		submitSamples(outBuf.data(), got);
	}

	int getSampleRate() const { return sampleRate; }

protected:
	// Wywolywana przez commitBatch() z gotowymi probkami float32 mono.
	// Klasy pochodne przesylaja je do backendu audio.
	virtual void submitSamples(const float* samples, int count) = 0;

private:
	// --- Resampler Sinc-Blackman -------------------------------------------

	// Zarejestruj wartosc wyjscia APU w chwili relTime [s] (wzgledem
	// ostatniego flush). Delta jest rozmywana przez jadro Sinc-Blackman
	// z sub-probkowa precyzja fazowa.
	void blipAddSample(double relTime, float value) {
		float delta = value - blipPrevOutput;
		if (delta == 0.0f) return;
		blipPrevOutput = value;

		double pos  = relTime * sampleRate;
		int    ipos = (int)pos;
		double frac = pos - ipos;

		int phase = (int)(frac * PHASES + 0.5);
		if (phase > PHASES) phase = PHASES;

		const float* krow  = kernel[phase];
		int          limit = (int)blipAccum.size();
		for (int i = 0; i < KERNEL_SIZE; i++) {
			int idx = ipos + i;
			if (idx >= 0 && idx < limit)
				blipAccum[idx] += krow[i] * delta;
		}
	}

	// Ile probek jest gotowych do odczytu do chwili endRelTime [s].
	int blipAvailable(double endRelTime) const {
		int n = (int)(endRelTime * sampleRate);
		if (n > (int)blipAccum.size()) n = (int)blipAccum.size();
		return n > 0 ? n : 0;
	}

	// Odczytaj probki (calkowanie akumulatora), przesuniecie bufora w lewo.
	int blipRead(float* out, int maxSamples, double endRelTime) {
		int count = blipAvailable(endRelTime);
		if (count > maxSamples) count = maxSamples;
		if (count <= 0) return 0;

		for (int i = 0; i < count; i++) {
			blipRunningSum += blipAccum[i];
			out[i]          = blipRunningSum;
		}

		int tail = (int)blipAccum.size() - count;
		std::memmove(blipAccum.data(), blipAccum.data() + count, tail * sizeof(float));
		std::fill(blipAccum.begin() + tail, blipAccum.end(), 0.0f);

		return count;
	}

	// Buduje jadro Sinc-Blackman znormalizowane per-faze.
	void buildKernel() {
		static constexpr float PI = 3.14159265358979f;

		for (int phase = 0; phase <= PHASES; phase++) {
			float sum = 0.0f;
			for (int i = 0; i < KERNEL_SIZE; i++) {
				float x = (float)(i - KERNEL_HALF + 1) - (float)phase / PHASES;
				float sinc = (std::abs(x) < 1e-6f)
					? 1.0f
					: std::sin(PI * x) / (PI * x);

				float n      = (float)i;
				float N      = (float)(KERNEL_SIZE - 1);
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

	// --- Pola -----------------------------------------------------------

	int    sampleRate = 0;
	double timeAccum  = 0.0;

	// Resampler
	std::vector<float> blipAccum;
	float              blipPrevOutput = 0.0f;
	float              blipRunningSum = 0.0f;
	float              kernel[PHASES + 1][KERNEL_SIZE] = {};

	// Filtry NES
	AudioFilter hpf90;
	AudioFilter hpf440;
	AudioFilter lpf14k;

	// Akumulator czasu biezacego batcha
	double batchTimeAcc = 0.0;

	std::vector<float> outBuf;
};
