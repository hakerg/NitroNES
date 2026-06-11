#pragma once
#include "Mapper004.h"
#include "MapperRegistry.h"

// ----------------------------------------------------------------------------
// Mapper 049 - Super HIK 4-in-1 (MMC3 multicart)
// ----------------------------------------------------------------------------
// Multicart reg @ $6000-$7FFF (zapisywalny tylko gdy PRG-RAM enabled):
//   [BBPP ...O]  B = blok 128 KB PRG+CHR, P = 32 KB PRG page (gdy O=0),
//                O = PRG mode (0 = 32 KB, 1 = MMC3)
// Bloki sa po 128 KB PRG i 128 KB CHR.
// W trybie MMC3 banki sa AND-owane do 16 (PRG 8 KB) / 128 (CHR 1 KB) i
// OR-owane z odpowiednia stala wynikajaca z bloku.
// ----------------------------------------------------------------------------
class Mapper049 : public Mapper004 {
public:
	using Mapper004::Mapper004;

	void reset() override {
		Mapper004::reset();
		multi = 0;
	}

	void cpuMapWrite(uint16_t addr, uint32_t& mapped, uint8_t data) override {
		if (addr >= 0x6000 && addr <= 0x7FFF) {
			multi = data;
			return;
		}
		Mapper004::cpuMapWrite(addr, mapped, data);
	}

	bool cpuMapRead(uint16_t addr, uint32_t& mapped, uint8_t& data) override {
		if (addr < 0x8000) return Mapper004::cpuMapRead(addr, mapped, data);
		uint8_t block = (multi >> 6) & 0x03;
		bool mmc3Mode = (multi & 0x01) != 0;
		uint8_t total8k = (uint8_t)(prgBanks * 2);
		if (!mmc3Mode) {
			// 32 KB swap, P z bitow 4..5
			uint8_t page32 = (multi >> 4) & 0x03;
			// bank w jednostkach 8 KB: block*16 + page32*4
			uint8_t bank8 = (uint8_t)((block * 16) + (page32 * 4) + ((addr >> 13) & 0x03));
			mapped = (uint32_t)mapper_helpers::maskBank(bank8, total8k) * 0x2000 + (addr & 0x1FFF);
			return true;
		}
		// MMC3 mode - banki ograniczone do bloku.
		uint32_t prgMmc3;
		if (!Mapper004::cpuMapRead(addr, prgMmc3, data)) return false;
		uint32_t bank8 = prgMmc3 >> 13;
		uint32_t off   = prgMmc3 & 0x1FFF;
		bank8 = (bank8 & 0x0F) | ((uint32_t)block * 16);
		if (total8k) bank8 %= total8k;
		mapped = bank8 * 0x2000 + off;
		return true;
	}

	bool ppuMapRead(uint16_t addr, uint32_t& mapped) override {
		if (!Mapper004::ppuMapRead(addr, mapped)) return false;
		if (chrBanks == 0) return true;
		uint8_t block = (multi >> 6) & 0x03;
		uint32_t bank1k = mapped >> 10;
		uint32_t off   = mapped & 0x03FF;
		bank1k = (bank1k & 0x7F) | ((uint32_t)block * 128);
		uint32_t total1k = (uint32_t)chrBanks * 8;
		if (total1k) bank1k %= total1k;
		mapped = bank1k * 0x0400 + off;
		return true;
	}

private:
	uint8_t multi = 0;
};

REGISTER_MAPPER(49, Mapper049)
