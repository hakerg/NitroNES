#pragma once
#include "MapperBase.h"
#include "MapperRegistry.h"
#include <array>

// ----------------------------------------------------------------------------
// Mapper 246 - Fong Shen Bang
// ----------------------------------------------------------------------------
// Range,Mask: $6000-$67FF, $6007
//   $6000-$6003: PRG regs (8 KB kazdy) -> $8000, $A000, $C000, $E000
//   $6004-$6007: CHR regs (2 KB kazdy) -> $0000, $0800, $1000, $1800
// SRAM dostepny od $6800-$7FFF (poza zakresem rejestrow).
// ----------------------------------------------------------------------------
class Mapper246 : public Mapper {
public:
	using Mapper::Mapper;

	void reset() override {
		prg.fill(0);
		prg[3] = 0xFF;          // wg nes_specs: $6003 = $FF na powerup
		chr.fill(0);
	}

	bool cpuMapRead(uint16_t addr, uint32_t& mapped, uint8_t&) override {
		if (addr < 0x8000) return false;
		uint8_t slot = (uint8_t)((addr >> 13) & 0x03);  // 0..3
		uint8_t bank = prg[slot];
		uint8_t maxBank = (uint8_t)(prgBanks * 2);
		if (maxBank) bank = (uint8_t)(bank % maxBank);
		mapped = (uint32_t)bank * 0x2000 + (addr & 0x1FFF);
		return true;
	}

	void cpuMapWrite(uint16_t addr, uint32_t&, uint8_t data) override {
		if (addr >= 0x6000 && addr <= 0x67FF) {
			uint8_t r = (uint8_t)(addr & 0x07);
			if (r < 4) prg[r]     = data;
			else       chr[r - 4] = data;
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

private:
	std::array<uint8_t, 4> prg{};
	std::array<uint8_t, 4> chr{};
};

REGISTER_MAPPER(246, Mapper246)
