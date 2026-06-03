#pragma once
#include "MapperBase.h"
#include "MapperRegistry.h"
#include <array>

// ----------------------------------------------------------------------------
// Mapper 154 - Namcot 88 (Devil Man)
// ----------------------------------------------------------------------------
// Jak Mapper 088, ale dodatkowo bit 6 z $8000 ustawia mirroring 1-screen A/B.
// ----------------------------------------------------------------------------
class Mapper154 : public Mapper {
public:
	using Mapper::Mapper;

	void reset() override {
		target = 0;
		regs.fill(0);
		mirrorMode = Mirroring::ONESCREEN_LO;
	}

	bool cpuMapRead(uint16_t addr, uint32_t& mapped, uint8_t&) override {
		if (addr < 0x8000) return false;
		uint8_t last = (uint8_t)(prgBanks * 2 - 1);
		uint8_t prev = (uint8_t)(prgBanks * 2 - 2);
		uint8_t bank;
		if (addr < 0xA000)      bank = regs[6] & 0x3F;
		else if (addr < 0xC000) bank = regs[7] & 0x3F;
		else if (addr < 0xE000) bank = prev;
		else                    bank = last;
		mapped = (uint32_t)bank * 0x2000 + (addr & 0x1FFF);
		return true;
	}

	void cpuMapWrite(uint16_t addr, uint32_t&, uint8_t data) override {
		if (addr < 0x8000) return;
		if ((addr & 0x8001) == 0x8000) {
			target     = data & 0x07;
			mirrorMode = (data & 0x40) ? Mirroring::ONESCREEN_HI : Mirroring::ONESCREEN_LO;
		} else if ((addr & 0x8001) == 0x8001) {
			regs[target] = data;
		}
	}

	bool ppuMapRead(uint16_t addr, uint32_t& mapped) override {
		if (addr > 0x1FFF) return false;
		uint8_t bank;
		uint32_t off;
		if (addr < 0x0800) {
			bank = (uint8_t)(regs[0] & 0x3E);
			off  = (uint32_t)bank * 0x0400 + (addr & 0x07FF);
		} else if (addr < 0x1000) {
			bank = (uint8_t)(regs[1] & 0x3E);
			off  = (uint32_t)bank * 0x0400 + (addr & 0x07FF);
		} else {
			uint8_t which = (uint8_t)((addr >> 10) & 0x03);
			bank = (uint8_t)(regs[2 + which] | 0x40);
			off  = (uint32_t)bank * 0x0400 + (addr & 0x03FF);
		}
		mapped = off;
		return true;
	}
	bool ppuMapWrite(uint16_t addr, uint32_t& mapped) override {
		return mapper_helpers::chrRamWrite(addr, mapped, chrBanks);
	}

	Mirroring mirror() const override { return mirrorMode; }
	bool hasDynamicMirror() const override { return true; }

private:
	uint8_t target = 0;
	std::array<uint8_t, 8> regs{};
	Mirroring mirrorMode = Mirroring::ONESCREEN_LO;
};

REGISTER_MAPPER(154, Mapper154)
