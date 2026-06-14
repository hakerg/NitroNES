#pragma once
#include <array>
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

class NESCoreBase : public IFrameConsumer, public ICPUBus {
public:
	bool    pal = false;
	double  speed = 1.0;
	bool    paused = false;

	explicit NESCoreBase(IEmulatorHost& host)
		: cpu(*this), host(host) {
		cpuRam.fill(0x00);
	}
	virtual ~NESCoreBase() = default;

	double getCPUClockRate() const {
		return pal ? NES::CPU_CLOCK_PAL : NES::CPU_CLOCK_NTSC;
	}

	void tickFrame(double& outDT) {
		if (paused) { outDT = 0.1; return; }
		pendingDT  = 0.0;
		frameReady = false;
		do {
			clockOneCycle();
		} while (!frameReady);
		outDT = pendingDT;
	}

	template <typename ConditionFunc>
	void tickWhile(ConditionFunc condition) {
		if (paused) return;
		while (condition()) {
			clockOneCycle();
		}
	}

	virtual void reset() {}
	virtual void shutdown() {}
	virtual void renderFrame() {}

	PPU2C02* getPPU() { return const_cast<PPU2C02*>(static_cast<const NESCoreBase*>(this)->getPPU()); }
	virtual const PPU2C02* getPPU() const { return nullptr; }
	bool hasPPU() const { return getPPU() != nullptr; }
	int  getCurrentScanline() const {
		const PPU2C02* p = getPPU();
		return p ? p->getScanline() : -1;
	}

	void onFrameComplete() override {
		frameReady = true;
		host.onFrameReady();
	}

	uint8_t cpuRead(uint16_t addr) override final {
		if (!cyclesEnabled) return memRead(addr);
		serviceDMA(addr);
		uint8_t v = 0;
		runSystemCycle([&] { v = memRead(addr); });
		return v;
	}

	void cpuWrite(uint16_t addr, uint8_t data) override final {
		if (!cyclesEnabled) { memWrite(addr, data); return; }
		runSystemCycle([&] { memWrite(addr, data); });
	}

	void cpuIrqAck() override final { mapperIrqAck(); }

protected:
	virtual uint8_t memRead(uint16_t addr) = 0;
	virtual void    memWrite(uint16_t addr, uint8_t data) = 0;
	virtual void    clockMapper() {}
	virtual float   mapperAudio() const { return 0.0f; }
	virtual bool    mapperIRQ() const { return false; }
	virtual void    mapperIrqAck() {}

	virtual void onPreStep() {}
	virtual void onCpuCycle(bool getCycle) { (void)getCycle; }
	virtual bool coreIRQLine() { return apu.irqAsserted() || mapperIRQ(); }

	void scheduleOAMDMA(uint8_t page) {
		dma.oamPending = true;
		dma.oamPage    = (uint16_t)page << 8;
	}

	bool isGetCycle() const { return getCycle; }

	CPU6502 cpu;
	APU2A03 apu;
	std::array<uint8_t, 2048> cpuRam;

private:
	IEmulatorHost& host;
	bool   frameReady    = false;
	bool   cyclesEnabled = false;
	double pendingDT     = 0.0;

	bool getCycle = true;

	struct DMA {
		bool     oamPending = false;  uint16_t oamPage = 0;
		bool     dmcPending = false;  uint16_t dmcAddr = 0;
		bool pending() const { return oamPending || dmcPending; }
	} dma;

	template <typename AccessFn>
	void runSystemCycle(AccessFn&& access) {
		PPU2C02* ppu = getPPU();
		if (ppu) { ppu->clock(); ppu->clock(); }
		clockMapper();
		apu.clock(getCycle);

		access();

		if (ppu) ppu->clock();

		if (apu.dmcNeedsSample() && !dma.dmcPending) {
			dma.dmcPending = true;
			dma.dmcAddr    = apu.dmcSampleAddress();
		}

		onCpuCycle(getCycle);

		getCycle = !getCycle;

		double dt = 1.0 / (getCPUClockRate() * speed);
		pendingDT += dt;
		host.pushAudioSample(apu.getOutputSample() + mapperAudio(), dt);
	}

	void dmaIdle(uint16_t cpuAddr) { runSystemCycle([&] { memRead(cpuAddr); }); }
	uint8_t dmaGet(uint16_t addr)  { uint8_t v = 0; runSystemCycle([&] { v = memRead(addr); }); return v; }

	void serviceDMA(uint16_t cpuAddr) {
		if (!dma.pending()) return;

		cpu.onRdyLow();

		if (dma.dmcPending) {
			dmaIdle(cpuAddr);
			dmaIdle(cpuAddr);
			if (!getCycle) dmaIdle(cpuAddr);
			uint8_t b = dmaGet(dma.dmcAddr);
			apu.loadDMCSample(b);
			dma.dmcPending = false;
		}

		if (dma.oamPending) {
			dmaIdle(cpuAddr);
			if (!getCycle) dmaIdle(cpuAddr);
			PPU2C02* ppu = getPPU();
			for (int i = 0; i < 256; i++) {
				uint8_t b = dmaGet(dma.oamPage + i);
				runSystemCycle([&] { if (ppu) ppu->oamDMAWrite(b); });
			}
			dma.oamPending = false;
		}
	}

	void clockOneCycle() {
		onPreStep();

		cyclesEnabled = true;
		cpu.tick();
		cyclesEnabled = false;

		PPU2C02* ppu = getPPU();
		cpu.setNMILine(ppu ? ppu->nmiLineLow() : false);
		cpu.setIRQ(coreIRQLine());
	}
};