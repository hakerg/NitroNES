#pragma once
#include "MapperBase.h"
#include "MapperRegistry.h"

// ----------------------------------------------------------------------------
// Mapper 062 - Super 700-in-1
// ----------------------------------------------------------------------------
// $8000-FFFF (latch z A i danych):
//   addr:  A~[..pp pppp MPOC CCCC]
//   data:     [.... ..cc]
// PRG reg 7 bit = (P<<6) | p (uzywamy w 16k pages); mode bit O.
// Mode 0: 32K bank, mode 1: 16K powtorzony.
// CHR reg 7 bit = (C<<2) | c, 8K @ $0000.
// Mirror: M (0=V, 1=H)
// ----------------------------------------------------------------------------
class Mapper062 : public Mapper {
public:
	using Mapper::Mapper;

	void reset() override { addrLatch = 0; dataLatch = 0; }

	bool cpuMapRead(uint16_t addr, uint32_t& mapped, uint8_t&) override {
		if (addr < 0x8000) return false;
		uint16_t p  = (addrLatch >> 8) & 0x3F;
		uint16_t P  = (addrLatch >> 6) & 0x01;
		uint8_t prgReg = (uint8_t)((P << 6) | p); // 16k unit
		bool mode32 = ((addrLatch >> 5) & 0x01) == 0;
		if (mode32) {
			uint8_t bank32 = (uint8_t)(prgReg >> 1);
			mapped = mapper_helpers::mapPrg32k(addr, bank32, prgBanks);
		} else {
			uint32_t off = (uint32_t)mapper_helpers::maskBank(prgReg, prgBanks) * 0x4000;
			mapped = off + (addr & 0x3FFF);
		}
		return true;
	}
	void cpuMapWrite(uint16_t addr, uint32_t&, uint8_t data) override {
		if (addr < 0x8000) return;
		addrLatch = addr;
		dataLatch = data;
	}
	bool ppuMapRead(uint16_t addr, uint32_t& mapped) override {
		if (addr > 0x1FFF) return false;
		uint8_t C = (uint8_t)(addrLatch & 0x1F);
		uint8_t c = (uint8_t)(dataLatch & 0x03);
		uint8_t bank = (uint8_t)((C << 2) | c);
		mapped = mapper_helpers::mapChr8k(addr, bank, chrBanks);
		return true;
	}
	bool ppuMapWrite(uint16_t addr, uint32_t& mapped) override {
		return mapper_helpers::chrRamWrite(addr, mapped, chrBanks);
	}

	Mirroring mirror() const override {
		return ((addrLatch >> 7) & 0x01) ? Mirroring::HORIZONTAL : Mirroring::VERTICAL;
	}
	bool hasDynamicMirror() const override { return true; }

private:
	uint16_t addrLatch = 0;
	uint8_t  dataLatch = 0;
};

REGISTER_MAPPER(62, Mapper062)
