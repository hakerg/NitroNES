#pragma once
#include "MapperBase.h"
#include "MapperRegistry.h"

// ----------------------------------------------------------------------------
// Mapper 058 — 68-in-1 / Study and Game 32-in-1 multicart
// Mapper 213 — alias (9999999-in-1 / 168-in-1); identyczna logika.
//
// Specyfikacja: docs/mapper_docs/058.txt
//
// Rejestr konfiguracyjny: zapis pod $8000-$FFFF
//   A~[.... .... MOCC CPPP]
//     bity 2-0 (P) — bank PRG (wybór banku)
//     bity 5-3 (C) — bank CHR 8KB @ $0000
//     bit  6   (O) — tryb PRG: 0=32KB, 1=2×16KB (fixowane)
//     bit  7   (M) — mirroring: 0=Vertical, 1=Horizontal
//
// PRG Mode 0: 32KB bank pod $8000-$FFFF  (bit 0 banku ignorowany → para)
// PRG Mode 1: 16KB bank pod $8000-$BFFF, następny 16KB bank pod $C000-$FFFF
// CHR:        zawsze 8KB bank pod $0000-$1FFF
// ----------------------------------------------------------------------------
class Mapper058 : public Mapper {
public:
	using Mapper::Mapper;

	void reset() override {
		prgBank = 0;
		chrBank = 0;
		prgMode = false;
		mirror_ = Mirroring::VERTICAL;
	}

	// ---- CPU ----------------------------------------------------------------

	bool cpuMapRead(uint16_t addr, uint32_t& mapped, uint8_t& /*data*/) override {
		if (addr < 0x8000) return false;

		if (prgMode) {
			// Mode 1: dwa niezależne okna 16KB
			if (addr < 0xC000)
				mapped = (uint32_t)prgBank * 0x4000u + (addr & 0x3FFF);
			else
				mapped = (uint32_t)(prgBank + 1) * 0x4000u + (addr & 0x3FFF);
		} else {
			// Mode 0: 32KB — prgBank & ~1 jako para
			uint32_t base = (uint32_t)(prgBank & ~1u) * 0x4000u;
			mapped = base + (addr & 0x7FFF);
		}
		return true;
	}

	void cpuMapWrite(uint16_t addr, uint32_t& /*mapped*/, uint8_t /*data*/) override {
		if (addr < 0x8000) return;

		// Dane z szyny adresowej (nie z danych) — typowe dla multicartów
		prgBank = (addr >> 0) & 0x07;           // A2-A0
		chrBank = (addr >> 3) & 0x07;           // A5-A3
		prgMode = (addr >> 6) & 0x01;           // A6
		mirror_ = ((addr >> 7) & 0x01)
				  ? Mirroring::HORIZONTAL
				  : Mirroring::VERTICAL;        // A7
		return; // nie mapujemy do ROM przy zapisie
	}

	// ---- PPU ----------------------------------------------------------------

	bool ppuMapRead(uint16_t addr, uint32_t& mapped) override {
		if (addr >= 0x2000) return false;
		mapped = (uint32_t)chrBank * 0x2000u + (addr & 0x1FFF);
		return true;
	}

	bool ppuMapWrite(uint16_t addr, uint32_t& mapped) override {
		if (addr >= 0x2000) return false;
		if (chrBanks == 0) { // CHR-RAM
			mapped = addr & 0x1FFF;
			return true;
		}
		return false;
	}

	// ---- Mirroring ----------------------------------------------------------

	Mirroring mirror() const override { return mirror_; }
	bool hasDynamicMirror()   const override { return true; }

private:
	uint8_t    prgBank = 0;
	uint8_t    chrBank = 0;
	bool       prgMode = false;
	Mirroring  mirror_ = Mirroring::VERTICAL;
};

REGISTER_MAPPER(58, Mapper058)
REGISTER_MAPPER_AS(213, Mapper058, alias213)
