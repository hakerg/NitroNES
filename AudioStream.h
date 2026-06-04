#pragma once
#include <vector>
#include <cmath>
#include <cstring>
#include "NESConst.h"
#include "AudioFilter.h"

class AudioStream {
public:
	static constexpr int CHANNELS = 1;
	static constexpr int KERNEL_SIZE = 16;
	static constexpr int KERNEL_HALF = KERNEL_SIZE / 2;
	static constexpr int PHASES = 128;

	AudioStream() = default;
	virtual ~AudioStream() = default;

	bool init(int rate) {
		sampleRate = rate;

		blipPrevOutput = 0.0f;
		blipRunningSum = 0.0f;
		buildKernel();

		hpf90 = AudioFilter(FilterType::HighPass, NES::AUDIO_HP1_CUTOFF, (float)sampleRate);
		hpf440 = AudioFilter(FilterType::HighPass, NES::AUDIO_HP2_CUTOFF, (float)sampleRate);
		lpf14k = AudioFilter(FilterType::LowPass, NES::AUDIO_LP_CUTOFF, (float)sampleRate);

		blipAccum.resize(KERNEL_SIZE + 1, 0.0f);
		timeAccum = 0.0;

		return true;
	}

	void addNESSample(double virtualDt, float value) {
		timeAccum += virtualDt * sampleRate;

		float delta = value - blipPrevOutput;
		if (delta != 0.0f) {
			blipPrevOutput = value;

			int ipos = (int)timeAccum;
			double frac = timeAccum - ipos;

			int phase = (int)(frac * PHASES + 0.5);
			if (phase > PHASES) phase = PHASES;

			if (ipos + KERNEL_SIZE >= blipAccum.size()) {
				blipAccum.resize(ipos + KERNEL_SIZE + 1, 0.0f);
			}

			const float* krow = kernel[phase];
			for (int i = 0; i < KERNEL_SIZE; i++) {
				blipAccum[ipos + i] += krow[i] * delta;
			}
		}

		while (timeAccum >= 1.0) {
			commitOutSample();
			timeAccum -= 1.0;
		}
	}

	void commitBatch() {
		if (outBuf.empty()) return;

		int got = (int)outBuf.size();
		for (int i = 0; i < got; i++) {
			float y = outBuf[i];
			y = hpf90.process(y);
			y = hpf440.process(y);
			y = lpf14k.process(y);
			y *= NES::AUDIO_VOLUME;
			outBuf[i] = y;
		}

		submitSamples(outBuf.data(), got);
		outBuf.clear();
	}

	int getSampleRate() const { return sampleRate; }

	int getQueuedSamples() const { return (int)outBuf.size(); }

protected:
	virtual void submitSamples(const float* samples, int count) = 0;

private:
	void commitOutSample() {
		blipRunningSum += blipAccum[0];
		outBuf.push_back(blipRunningSum);

		int size = (int)blipAccum.size();
		std::memmove(blipAccum.data(), blipAccum.data() + 1, (size - 1) * sizeof(float));
		blipAccum[size - 1] = 0.0f;
	}

	void buildKernel() {
		static constexpr float PI = 3.14159265358979f;

		for (int phase = 0; phase <= PHASES; phase++) {
			float sum = 0.0f;
			for (int i = 0; i < KERNEL_SIZE; i++) {
				float x = (float)(i - KERNEL_HALF + 1) - (float)phase / PHASES;
				float sinc = (std::abs(x) < 1e-6f)
					? 1.0f
					: std::sin(PI * x) / (PI * x);

				float n = (float)i;
				float N = (float)(KERNEL_SIZE - 1);
				float window = 0.42f
					- 0.5f * std::cos(2.0f * PI * n / N)
					+ 0.08f * std::cos(4.0f * PI * n / N);

				kernel[phase][i] = sinc * window;
				sum += kernel[phase][i];
			}
			if (sum != 0.0f)
				for (int i = 0; i < KERNEL_SIZE; i++)
					kernel[phase][i] /= sum;
		}
	}

	int    sampleRate = 0;
	double timeAccum = 0.0;

	std::vector<float> blipAccum;
	float              blipPrevOutput = 0.0f;
	float              blipRunningSum = 0.0f;
	float              kernel[PHASES + 1][KERNEL_SIZE] = {};

	AudioFilter hpf90;
	AudioFilter hpf440;
	AudioFilter lpf14k;

	std::vector<float> outBuf;
};