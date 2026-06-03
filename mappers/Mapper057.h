#pragma once
#include "MapperBase.h"
#include "MapperRegistry.h"

// ----------------------------------------------------------------------------
// Mapper 057 - GK 47-in-1 / 6-in-1 (SuperGK)
// ----------------------------------------------------------------------------
// Range,Mask:   $8000-FFFF, $8800
//   $8000: [.H.. .AAA]   H = bit 4 CHR reg,  A = OR z B
//   $8800: [PPPO MBBB]   P = PRG reg, O = PRG mode (0=16K duplikat, 1=32K),
//                        M = mirror (0=V,1=H), B = OR z A
// CHR: 4-bitowy bank 8K @ $0000 = (H<<3) | (A|B)
// PRG mode 0: dwie kopie 16K wybrane przez P; mode 1: 32K (P>>1).
// ----------------------------------------------------------------------------
class Mapper057 : public Mapper {
public:
	using Mapper::Mapper;

	void reset() override { reg8000 = reg8800 = 0; }

	bool cpuMapRead(uint16_t addr, uint32_t& mapped, uint8_t&) override {
		if (addr < 0x8000) return false;
		uint8_t prgReg = (reg8800 >> 5) & 0x07;
		bool mode32 = (reg8800 & 0x10) != 0;
		if (mode32) {
			uint8_t bank = (uint8_t)(prgReg >> 1);
			mapped = mapper_helpers::mapPrg32k(addr, bank, prgBanks);
		} else {
			mapped = mapper_helpers::mapPrg16k_fixedHi(addr, prgReg, prgBanks);
			if (addr >= 0xC000) {
				// mode 0 duplikuje 16K
				mapped = (uint32_t)mapper_helpers::maskBank(prgReg, prgBanks) * 0x4000 + (addr & 0x3FFF);
			}
		}
		return true;
	}
	void cpuMapWrite(uint16_t addr, uint32_t&, uint8_t data) override {
		if (addr < 0x8000) return;
		uint16_t r = addr & 0x8800;
		if (r == 0x8000)      reg8000 = data;
		else if (r == 0x8800) reg8800 = data;
	}
	bool ppuMapRead(uint16_t addr, uint32_t& mapped) override {
		if (addr > 0x1FFF) return false;
		uint8_t lo = (uint8_t)((reg8000 & 0x07) | (reg8800 & 0x07));
		uint8_t hi = (reg8000 >> 6) & 0x01; // bit H to bit 6
		uint8_t bank = (uint8_t)((hi << 3) | lo);
		mapped = mapper_helpers::mapChr8k(addr, bank, chrBanks);
		return true;
	}
	bool ppuMapWrite(uint16_t addr, uint32_t& mapped) override {
		return mapper_helpers::chrRamWrite(addr, mapped, chrBanks);
	}

	Mirroring mirror() const override {
		return (reg8800 & 0x08) ? Mirroring::HORIZONTAL : Mirroring::VERTICAL;
	}
	bool hasDynamicMirror() const override { return true; }

private:
	uint8_t reg8000 = 0, reg8800 = 0;
};

REGISTER_MAPPER(57, Mapper057)
