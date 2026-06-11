#pragma once
#include "MapperBase.h"
#include "MapperRegistry.h"

// ----------------------------------------------------------------------------
// Mapper 230 - 22-in-1 (Contra / multicart, reset-driven)
// ----------------------------------------------------------------------------
// 2 PRG chipy: chip0 (128 KB) z Contra, chip1 (512 KB) z multicartem.
// Reset toggluje tryb. Bez wsparcia softreset cycle, startujemy w trybie multicart.
// Contra Mode  $8000-FFFF: [.... .PPP] - 16K page z chip0 + fixed page 7 @ $C000.
// Multi Mode   $8000-FFFF: [.MOP PPPP]
//   M=mirror (0=H,1=V), O=PRG mode (0=32K, 1=16K powtorzony), P=PRG page
// ----------------------------------------------------------------------------
class Mapper230 : public Mapper {
public:
	using Mapper::Mapper;

	void reset() override {
		contraMode = !contraMode;  // toggluj przy resecie
		reg = 0;
		mirrorMode = contraMode ? Mirroring::VERTICAL : Mirroring::HORIZONTAL;
	}

	bool cpuMapRead(uint16_t addr, uint32_t& mapped, uint8_t&) override {
		if (addr < 0x8000) return false;
		if (contraMode) {
			uint8_t page = (addr < 0xC000) ? (uint8_t)(reg & 0x07) : 7;
			uint32_t off = (uint32_t)page * 0x4000 + (addr & 0x3FFF);
			mapped = off; // chip 0 zaczyna sie na offsecie 0
			return true;
		}
		// multicart - chip 1 zaczyna sie po 128 KB
		uint32_t chipBase = 128 * 1024;
		bool mode16 = (reg & 0x20) != 0;
		uint8_t p = (uint8_t)(reg & 0x1F);
		uint32_t off;
		if (mode16) off = chipBase + (uint32_t)p * 0x4000 + (addr & 0x3FFF);
		else        off = chipBase + (uint32_t)(p >> 1) * 0x8000 + (addr & 0x7FFF);
		uint32_t romSize = (uint32_t)prgBanks * 0x4000;
		if (romSize) off %= romSize;
		mapped = off;
		return true;
	}

	void cpuMapWrite(uint16_t addr, uint32_t&, uint8_t data) override {
		if (addr < 0x8000) return;
		reg = data;
		if (!contraMode)
			mirrorMode = (data & 0x40) ? Mirroring::VERTICAL : Mirroring::HORIZONTAL;
	}

	bool ppuMapRead(uint16_t addr, uint32_t& mapped) override {
		if (addr > 0x1FFF) return false;
		mapped = addr;
		return true;
	}
	bool ppuMapWrite(uint16_t addr, uint32_t& mapped) override {
		return mapper_helpers::chrRamWrite(addr, mapped, chrBanks);
	}
	Mirroring mirror() const override { return mirrorMode; }
	bool hasDynamicMirror() const override { return true; }

private:
	uint8_t reg = 0;
	bool contraMode = true;  // pierwszy reset() ustawi false -> multicart przy starcie
	Mirroring mirrorMode = Mirroring::HORIZONTAL;
};

REGISTER_MAPPER(230, Mapper230)
