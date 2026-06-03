#pragma once
#include "MapperBase.h"
#include "MapperRegistry.h"

// ----------------------------------------------------------------------------
// Mapper 002 - UxROM
// ----------------------------------------------------------------------------
class Mapper002 : public Mapper {
public:
	Mapper002(uint8_t prg, uint8_t chr) : Mapper(prg, chr) { prgBankLo = 0; prgBankHi = prg - 1; }

	void reset() override { prgBankLo = 0; prgBankHi = prgBanks - 1; }

	bool cpuMapRead(uint16_t addr, uint32_t& mapped, uint8_t&) override {
		if (addr >= 0x8000 && addr < 0xC000) {
			mapped = (uint32_t)prgBankLo * 0x4000 + (addr & 0x3FFF);
			return true;
		}
		if (addr >= 0xC000) {
			mapped = (uint32_t)prgBankHi * 0x4000 + (addr & 0x3FFF);
			return true;
		}
		return false;
	}
	void cpuMapWrite(uint16_t addr, uint32_t&, uint8_t data) override {
		if (addr >= 0x8000) { prgBankLo = data & 0x0F; return; }

	}
	bool ppuMapRead(uint16_t addr, uint32_t& mapped) override {
		if (addr <= 0x1FFF) { mapped = addr; return true; }
		return false;
	}
	bool ppuMapWrite(uint16_t addr, uint32_t& mapped) override {
		return mapper_helpers::chrRamWrite(addr, mapped, chrBanks);
	}

private:
	uint8_t prgBankLo = 0, prgBankHi = 0;
};

REGISTER_MAPPER(2, Mapper002)
