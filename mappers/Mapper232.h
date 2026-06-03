#pragma once
#include "MapperBase.h"
#include "MapperRegistry.h"

// ----------------------------------------------------------------------------
// Mapper 232 - Camerica/Codemasters Quattro
// ----------------------------------------------------------------------------
// $8000-BFFF: [...B B...]   PRG Block Select (64 KB block)
// $C000-FFFF: [.... ..PP]   PRG Page Select (16 KB w obrebie bloku)
// PRG: $8000-BFFF = block|page, $C000-FFFF = block|3 (fixed last w bloku)
// CHR-RAM 8 KB.
// ----------------------------------------------------------------------------
class Mapper232 : public Mapper {
public:
	using Mapper::Mapper;

	void reset() override { block = 0; page = 0; }

	bool cpuMapRead(uint16_t addr, uint32_t& mapped, uint8_t&) override {
		if (addr < 0x8000) return false;
		uint8_t p = (addr < 0xC000) ? page : (uint8_t)0x03;
		uint8_t bank = (uint8_t)((block << 2) | (p & 0x03));
		bank = mapper_helpers::maskBank(bank, prgBanks);
		mapped = (uint32_t)bank * 0x4000 + (addr & 0x3FFF);
		return true;
	}

	void cpuMapWrite(uint16_t addr, uint32_t&, uint8_t data) override {
		if (addr < 0x8000) return;
		if (addr < 0xC000) block = (uint8_t)((data >> 3) & 0x03);
		else                page  = (uint8_t)(data & 0x03);
	}

	bool ppuMapRead(uint16_t addr, uint32_t& mapped) override {
		if (addr > 0x1FFF) return false;
		mapped = addr;
		return true;
	}
	bool ppuMapWrite(uint16_t addr, uint32_t& mapped) override {
		return mapper_helpers::chrRamWrite(addr, mapped, chrBanks);
	}

private:
	uint8_t block = 0, page = 0;
};

REGISTER_MAPPER(232, Mapper232)
