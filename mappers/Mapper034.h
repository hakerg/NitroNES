#pragma once
#include "MapperBase.h"
#include "MapperRegistry.h"

// Mapper 034 - BNROM oraz NINA-001 dziel¹ ten numer. Implementujemy obie
// ga³êzie: pisanie do $7FFD/$7FFE/$7FFF (NINA-001) i pisanie do $8000+
// (BNROM, z bus conflicts).
class Mapper034 : public Mapper {
public:
	using Mapper::Mapper;
	void reset() override { prg = 0; chr0 = 0; chr1 = 1; }
	bool hasBusConflicts() const override { return true; }

	bool cpuMapRead(uint16_t a, uint32_t& mapped, uint8_t&) override {
		if (a < 0x8000) return false;
		mapped = (uint32_t)prg * 0x8000 + (a & 0x7FFF);
		return true;
	}
	void cpuMapWrite(uint16_t a, uint32_t&, uint8_t data) override {
		if (a == 0x7FFD) { prg  = data; return; }
		if (a == 0x7FFE) { chr0 = data; return; }
		if (a == 0x7FFF) { chr1 = data; return; }
		if (a >= 0x8000) { prg  = data; return; } // BNROM

	}
	bool ppuMapRead(uint16_t a, uint32_t& mapped) override {
		if (a > 0x1FFF) return false;
		if (chrBanks == 0) { mapped = a; return true; }
		if (a < 0x1000) mapped = (uint32_t)chr0 * 0x1000 + (a & 0x0FFF);
		else            mapped = (uint32_t)chr1 * 0x1000 + (a & 0x0FFF);
		return true;
	}
	bool ppuMapWrite(uint16_t a, uint32_t& mapped) override {
		return mapper_helpers::chrRamWrite(a, mapped, chrBanks);
	}
private:
	uint8_t prg = 0, chr0 = 0, chr1 = 1;
};

REGISTER_MAPPER(34, Mapper034)
