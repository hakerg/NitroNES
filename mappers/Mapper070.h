#pragma once
#include "MapperBase.h"
#include "MapperRegistry.h"

// Mapper 070 - Bandai 74161/74161/32. PRG select 16k @ $8000, CHR 8k.
class Mapper070 : public Mapper {
public:
	using Mapper::Mapper;
	void reset() override { prg = 0; chr = 0; }
	bool cpuMapRead(uint16_t a, uint32_t& mapped, uint8_t&) override {
		if (a < 0x8000) return false;
		if (a < 0xC000) mapped = (uint32_t)prg * 0x4000 + (a & 0x3FFF);
		else            mapped = (uint32_t)(prgBanks - 1) * 0x4000 + (a & 0x3FFF);
		return true;
	}
	void cpuMapWrite(uint16_t a, uint32_t&, uint8_t data) override {
		if (a < 0x8000) return;
		prg = (data >> 4) & 0x07;
		chr = data & 0x0F;

	}
	bool ppuMapRead(uint16_t a, uint32_t& mapped) override {
		if (a > 0x1FFF) return false;
		mapped = (uint32_t)chr * 0x2000 + (a & 0x1FFF);
		return true;
	}
	bool ppuMapWrite(uint16_t a, uint32_t& mapped) override {
		return mapper_helpers::chrRamWrite(a, mapped, chrBanks);
	}
private:
	uint8_t prg = 0, chr = 0;
};

REGISTER_MAPPER(70, Mapper070)
