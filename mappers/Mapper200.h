#pragma once
#include "MapperBase.h"
#include "MapperRegistry.h"

// ----------------------------------------------------------------------------
// Mapper 200 - 1200-in-1 / 36-in-1
// ----------------------------------------------------------------------------
// Adres zapisu wybiera bank: A~[.... .... .... MRRR]
//   M = mirroring (0=V, 1=H)
//   R = PRG (16K mirror) i CHR (8K)
// $8000-FFFF mapuje ten sam 16 KB bank dwa razy (lustrzane).
// ----------------------------------------------------------------------------
class Mapper200 : public Mapper {
public:
	using Mapper::Mapper;

	void reset() override { bank = 0; mirrorMode = Mirroring::VERTICAL; }

	bool cpuMapRead(uint16_t addr, uint32_t& mapped, uint8_t&) override {
		if (addr < 0x8000) return false;
		uint8_t b = mapper_helpers::maskBank(bank, prgBanks);
		mapped = (uint32_t)b * 0x4000 + (addr & 0x3FFF);
		return true;
	}
	void cpuMapWrite(uint16_t addr, uint32_t&, uint8_t /*data*/) override {
		if (addr < 0x8000) return;
		bank       = (uint8_t)(addr & 0x07);
		mirrorMode = (addr & 0x08) ? Mirroring::HORIZONTAL : Mirroring::VERTICAL;
	}
	bool ppuMapRead(uint16_t addr, uint32_t& mapped) override {
		if (addr > 0x1FFF) return false;
		mapped = mapper_helpers::mapChr8k(addr, bank, chrBanks);
		return true;
	}
	bool ppuMapWrite(uint16_t addr, uint32_t& mapped) override {
		return mapper_helpers::chrRamWrite(addr, mapped, chrBanks);
	}
	Mirroring mirror() const override { return mirrorMode; }
	bool hasDynamicMirror() const override { return true; }

private:
	uint8_t bank = 0;
	Mirroring mirrorMode = Mirroring::VERTICAL;
};

REGISTER_MAPPER(200, Mapper200)
