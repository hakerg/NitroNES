#pragma once
#include <cstdint>
#include <array>
#include <atomic>
#include <functional>
#include <mutex>
#include <string>

#include "NESConst.h"
#include "CPU6502.h"
#include "APU2A03.h"

class NESCoreBase {
public:
	bool    pal = false;
	double  speed = 1.0;
	bool    paused = false;

	NESCoreBase()
		: cpu([this](uint16_t a) { return cpuRead(a); },
			  [this](uint16_t a, uint8_t d) { cpuWrite(a, d); }) {
		cpuRam.fill(0x00);
	}
	virtual ~NESCoreBase() = default;

	std::function<void(float sample, double dt)> onAudioSample;

	double getCPUClockRate() {
		return pal ? NES::CPU_CLOCK_PAL : NES::CPU_CLOCK_NTSC;
	}

	void tickFrame(double& outDT) {
		outDT = 0.0;
		if (paused) return;

		std::lock_guard<std::mutex> lock(tickMutex);
		bool frameReady = false;
		onFrameComplete = [&frameReady]() { frameReady = true; };
		do {
			double dt;
			tick(dt);
			outDT += dt;
		} while (!frameReady);
		onFrameComplete = nullptr;
	}

	void tickWhile(std::function<bool()> condition) {
		if (paused) return;

		std::lock_guard<std::mutex> lock(tickMutex);
		while (!paused && condition()) {
			double dt;
			tick(dt);
		}
	}

	virtual bool loadFile(const std::string& path) = 0;

	virtual std::string windowTitle(const std::string& filename) const = 0;

	virtual void shutdown() {}
	virtual void renderFrame() {}

	// --- Dostęp do stanu PPU ---
	virtual bool hasPPU() const { return false; }
	virtual int getCurrentScanline() const { return -1; }

	// --- Eventy wejściowe
	virtual void onGamepadAdded(uint32_t /*joystickId*/)   {}
	virtual void onGamepadRemoved(uint32_t /*joystickId*/) {}
	virtual void onSpacePressed()                          {}
	virtual void onRightPressed()                          {}
	virtual void onLeftPressed()                           {}

protected:
	virtual void clockOneCycle(float& outAudioSample) = 0;

	virtual uint8_t cpuRead(uint16_t addr) = 0;
	virtual void    cpuWrite(uint16_t addr, uint8_t data) = 0;

	CPU6502 cpu;
	APU2A03 apu;
	std::array<uint8_t, 2048> cpuRam;
	std::function<void()> onFrameComplete;

private:
	std::mutex tickMutex;

	void tick(double& outDT) {
		outDT = (1.0 / getCPUClockRate()) / speed;

		if (!paused) {
			float audioSample = 0.0f;
			clockOneCycle(audioSample);
			if (onAudioSample) onAudioSample(audioSample, outDT);
		}
	}
};