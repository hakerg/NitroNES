#pragma once
#include "MapperBase.h"
#include "MapperRegistry.h"

// Mapper 094 - Senjou no Ookami. 16k PRG @ $8000 (bity 2-4 z zapisu).
class Mapper094 : public Mapper {
public:
	using Mapper::Mapper;
	void reset() override { prg = 0; }
	bool hasBusConflicts() const override { return true; }
	bool cpuMapRead(uint16_t a, uint32_t& mapped, uint8_t&) override {
		if (a < 0x8000) return false;
		if (a < 0xC000) mapped = (uint32_t)prg * 0x4000 + (a & 0x3FFF);
		else            mapped = (uint32_t)(prgBanks - 1) * 0x4000 + (a & 0x3FFF);
		return true;
	}
	void cpuMapWrite(uint16_t a, uint32_t&, uint8_t data) override {
		if (a < 0x8000) return;
		prg = (data >> 2) & 0x07;

	}
	bool ppuMapRead(uint16_t a, uint32_t& mapped) override {
		if (a > 0x1FFF) return false;
		mapped = a; return true;
	}
	bool ppuMapWrite(uint16_t a, uint32_t& mapped) override {
		return mapper_helpers::chrRamWrite(a, mapped, chrBanks);
	}
private:
	uint8_t prg = 0;
};

REGISTER_MAPPER(94, Mapper094)
