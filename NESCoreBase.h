#pragma once
#include <cstdint>
#include <array>
#include <atomic>
#include <functional>
#include <string>

#include "NESConst.h"
#include "CPU6502.h"
#include "APU2A03.h"

class NESCoreBase {
public:
	NESCoreBase()
		: cpu([this](uint16_t a) { return cpuRead(a); },
			  [this](uint16_t a, uint8_t d) { cpuWrite(a, d); }) {
		cpuRam.fill(0x00);
	}
	virtual ~NESCoreBase() = default;

	void setResetAudio(std::function<void()> f) { resetAudio = std::move(f); }

	void tick(float& outSample, double& outDt) {
		if (!paused) clockOneCycle();
		outSample = getAudioSample();
		const double hwClock = pal ? NES::CPU_CLOCK_PAL : NES::CPU_CLOCK_NTSC;
		outDt = (1.0 / hwClock) / emuSpeed;
	}

	void   setSpeed(double spd) { emuSpeed = spd; }
	double getSpeed() const { return emuSpeed; }

	void togglePause()      { paused = !paused; }
	void setPaused(bool p)  { paused = p; }
	bool isPaused() const   { return paused; }

	virtual bool loadFile(const std::string& path) = 0;

	virtual std::string windowTitle(const std::string& filename) const = 0;
	virtual void        defaultWindowSize(int& w, int& h) const = 0;
	virtual bool        windowResizable() const { return false; }

	virtual void shutdown() {}
	virtual void renderFrame() {}

	// --- Eventy wejściowe (wywoływane z pętli głównej) --------------------
	virtual void onGamepadAdded(uint32_t /*joystickId*/)   {}
	virtual void onGamepadRemoved(uint32_t /*joystickId*/) {}
	virtual void onSpacePressed()                          {}
	virtual void onRightPressed()                          {}
	virtual void onLeftPressed()                           {}

protected:
	virtual void    clockOneCycle() = 0;
	virtual float   getAudioSample() = 0;

	virtual uint8_t cpuRead(uint16_t addr) = 0;
	virtual void    cpuWrite(uint16_t addr, uint8_t data) = 0;

	CPU6502 cpu;
	APU2A03 apu;
	std::array<uint8_t, 2048> cpuRam;
	bool    pal = false;
	double  emuSpeed = 1.0;
	bool    paused = false;
	std::function<void()> resetAudio;
};