#pragma once
#include "MapperBase.h"
#include "MapperRegistry.h"

// Mapper 140 - Jaleco JF-11/JF-14. Rejestr w $6000-7FFF.
class Mapper140 : public Mapper {
public:
	using Mapper::Mapper;
	void reset() override { prg = 0; chr = 0; }
	bool cpuMapRead(uint16_t a, uint32_t& mapped, uint8_t&) override {
		if (a < 0x8000) return false;
		mapped = (uint32_t)prg * 0x8000 + (a & 0x7FFF);
		return true;
	}
	void cpuMapWrite(uint16_t a, uint32_t&, uint8_t data) override {
		if (a >= 0x6000 && a <= 0x7FFF) {
			prg = (data >> 4) & 0x03;
			chr = data & 0x0F;

		}

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

REGISTER_MAPPER(140, Mapper140)
