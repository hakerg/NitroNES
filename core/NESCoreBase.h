#pragma once
#include <cstdint>
#include <array>
#include <atomic>
#include <string>

#include "NESConst.h"
#include "CPU6502.h"
#include "APU2A03.h"
#include "PPU2C02.h"

class IEmulatorHost {
public:
	virtual ~IEmulatorHost() = default;
	virtual void pushAudioSample(float sample, double dt) = 0;
	virtual void onFrameReady() = 0;
};

class NESCoreBase : public IFrameConsumer {
public:
	bool    pal = false;
	double  speed = 1.0;
	bool    paused = false;

	NESCoreBase(ICPUBus& bus, IEmulatorHost& host)
		: cpu(bus), host(host) {
		cpuRam.fill(0x00);
	}
	virtual ~NESCoreBase() = default;

	double getCPUClockRate() {
		return pal ? NES::CPU_CLOCK_PAL : NES::CPU_CLOCK_NTSC;
	}

	void tickFrame(double& outDT) {
		outDT = 0.0;
		if (paused) return;

		frameReady = false;
		do {
			double dt;
			tick(dt);
			outDT += dt;
		} while (!frameReady);
	}

	template <typename ConditionFunc>
	void tickWhile(ConditionFunc condition) {
		if (paused) return;

		while (!paused && condition()) {
			double dt;
			tick(dt);
		}
	}

	virtual bool loadFile(const std::string& path) = 0;
	virtual void shutdown() {}
	virtual void renderFrame() {}

	// --- Dostęp do stanu PPU ---
	virtual bool hasPPU() const { return false; }
	virtual int getCurrentScanline() const { return -1; }

protected:
	virtual void clockOneCycle(float& outAudioSample) = 0;

	void onFrameComplete() override {
		frameReady = true;
		host.onFrameReady();
	}

	CPU6502 cpu;
	APU2A03 apu;
	std::array<uint8_t, 2048> cpuRam;

private:
	IEmulatorHost& host;
	bool frameReady = false;

	void tick(double& outDT) {
		outDT = (1.0 / getCPUClockRate()) / speed;

		if (!paused) {
			float audioSample = 0.0f;
			clockOneCycle(audioSample);
			host.pushAudioSample(audioSample, outDT);
		}
	}
};