#pragma once
#include <cstdint>
#include <array>
#include <atomic>
#include <functional>
#include <string>
#include <SDL3/SDL.h>

#include "NESConst.h"
#include "CPU6502.h"
#include "APU2A03.h"

class NESCoreBase {
public:
	struct Host {
		std::function<void()> resetAudio;
	};

	NESCoreBase()
		: cpu([this](uint16_t a) { return cpuRead(a);  },
			[this](uint16_t a, uint8_t d) { cpuWrite(a, d);     })
	{
		cpuRam.fill(0x00);
	}
	virtual ~NESCoreBase() = default;

	void setHost(Host h) { host = std::move(h); }

	virtual void clockOneCycle(float& outSample, double& outDt) = 0;

	void   setSpeed(double spd) { emuSpeed = spd; }
	double getSpeed() const { return emuSpeed; }

	virtual bool loadFile(const std::string& path) = 0;

	virtual std::string windowTitle(const std::string& filename) const = 0;
	virtual void        defaultWindowSize(int& w, int& h) const = 0;
	virtual bool        windowResizable() const { return false; }

	virtual void initVideo(SDL_Window* /*window*/) {}
	virtual void shutdown() {}

	virtual bool handleEvent(const SDL_Event& /*ev*/, bool& /*paused*/) { return false; }

protected:
	virtual uint8_t cpuRead(uint16_t addr) = 0;
	virtual void    cpuWrite(uint16_t addr, uint8_t data) = 0;

	CPU6502 cpu;
	APU2A03 apu;
	std::array<uint8_t, 2048> cpuRam;
	bool    pal = false;
	double  emuSpeed = 1.0;
	Host    host;
};