#pragma once
#include "MapperBase.h"
#include "MapperRegistry.h"
#include <array>

// ----------------------------------------------------------------------------
// Mapper 091 - Street Fighter III / Super Mario & Sonic 2 pirate
// ----------------------------------------------------------------------------
// Range,Mask: $6000-7FFF, $7003
//   $6000-6003: CHR Regs (2K kazdy, @ $0000..$1FFF)
//   $7000-7001: [.... PPPP] PRG Regs (8K @ $8000 i $A000)
//   $7002: IRQ Stop  (= write $E000 + $E000 MMC3 effect)
//   $7003: IRQ Start (= write $07 do $C000, write do $C001 i $E001)
// PRG: { $7000, $7001, -2, -1 }
// IRQ: zachowanie jak MMC3 z fixed reload = 7.
// ----------------------------------------------------------------------------
class Mapper091 : public Mapper {
public:
	using Mapper::Mapper;

	void reset() override {
		chr.fill(0); prg.fill(0);
		irqCounter = 0; irqEnable = false; irqActive = false; irqReloadFlag = false;
		lastA12 = false;
	}

	bool cpuMapRead(uint16_t addr, uint32_t& mapped, uint8_t&) override {
		if (addr < 0x8000) return false;
		uint8_t last = (uint8_t)(prgBanks * 2 - 1);
		uint8_t prev = (uint8_t)(prgBanks * 2 - 2);
		uint8_t bank;
		if (addr < 0xA000)      bank = prg[0];
		else if (addr < 0xC000) bank = prg[1];
		else if (addr < 0xE000) bank = prev;
		else                    bank = last;
		mapped = (uint32_t)bank * 0x2000 + (addr & 0x1FFF);
		return true;
	}
	void cpuMapWrite(uint16_t addr, uint32_t&, uint8_t data) override {
		if (addr < 0x6000 || addr >= 0x8000) return;
		uint16_t r = addr & 0x7003;
		switch (r) {
			case 0x6000: chr[0] = data; break;
			case 0x6001: chr[1] = data; break;
			case 0x6002: chr[2] = data; break;
			case 0x6003: chr[3] = data; break;
			case 0x7000: prg[0] = data & 0x0F; break;
			case 0x7001: prg[1] = data & 0x0F; break;
			case 0x7002: irqEnable = false; irqActive = false; break;
			case 0x7003:
				irqEnable = true;
				irqReloadFlag = true;
				irqCounter = 0;
				break;
		}
	}
	bool ppuMapRead(uint16_t addr, uint32_t& mapped) override {
		if (addr > 0x1FFF) return false;
		uint8_t slot = (uint8_t)((addr >> 11) & 0x03);
		mapped = (uint32_t)chr[slot] * 0x0800 + (addr & 0x07FF);
		return true;
	}
	bool ppuMapWrite(uint16_t addr, uint32_t& mapped) override {
		return mapper_helpers::chrRamWrite(addr, mapped, chrBanks);
	}

	void scanline() override { tickIrq(); }
	void clockA12(bool a12High) override {
		if (a12High && !lastA12) tickIrq();
		lastA12 = a12High;
	}
	bool irqState() const override { return irqActive; }
	void irqClear() override { irqActive = false; }

private:
	void tickIrq() {
		if (irqCounter == 0 || irqReloadFlag) { irqCounter = 7; irqReloadFlag = false; }
		else irqCounter--;
		if (irqCounter == 0 && irqEnable) irqActive = true;
	}

	std::array<uint8_t, 4> chr{};
	std::array<uint8_t, 2> prg{};
	uint16_t irqCounter = 0;
	bool irqEnable = false, irqActive = false, irqReloadFlag = false, lastA12 = false;
};

REGISTER_MAPPER(91, Mapper091)
