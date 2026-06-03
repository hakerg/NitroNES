#pragma once
#include "MapperBase.h"
#include "MapperRegistry.h"

// ----------------------------------------------------------------------------
// Mapper 242 - Wai Xing Zhan Shi
// ----------------------------------------------------------------------------
// $8000-FFFF: A~[.... .... .PPP P.M.]
//   P=32K PRG, M=mirror (0=V, 1=H). Brak bus conflicts.
// CHR-RAM 8 KB.
// ----------------------------------------------------------------------------
class Mapper242 : public Mapper {
public:
	using Mapper::Mapper;

	void reset() override { prg = 0; mirrorMode = Mirroring::VERTICAL; }

	bool cpuMapRead(uint16_t addr, uint32_t& mapped, uint8_t&) override {
		if (addr < 0x8000) return false;
		mapped = mapper_helpers::mapPrg32k(addr, prg, prgBanks);
		return true;
	}
	void cpuMapWrite(uint16_t addr, uint32_t&, uint8_t /*data*/) override {
		if (addr < 0x8000) return;
		prg        = (uint8_t)((addr >> 3) & 0x0F);
		mirrorMode = (addr & 0x02) ? Mirroring::HORIZONTAL : Mirroring::VERTICAL;
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
	uint8_t prg = 0;
	Mirroring mirrorMode = Mirroring::VERTICAL;
};

REGISTER_MAPPER(242, Mapper242)
