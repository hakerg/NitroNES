#pragma once
#include "MapperBase.h"
#include "MapperRegistry.h"
#include <array>

// ----------------------------------------------------------------------------
// Mapper 004 - MMC3 / TxROM
// ----------------------------------------------------------------------------
class Mapper004 : public Mapper {
public:
	Mapper004(uint8_t prg, uint8_t chr) : Mapper(prg, chr) {
		pPRGBank.fill(0);
		pCHRBank.fill(0);
		reset();
	}

	void reset() override {
		targetReg = 0; prgMode = false; chrInversion = false;
		mirrorMode = Mirroring::HORIZONTAL;
		irqActive = false; irqEnable = false; irqReloadFlag = false;
		irqCounter = 0; irqReload = 0;
		for (auto& r : pRegister) r = 0;
		pCHRBank.fill(0);
		// PRG: domy�lnie {R6,R7,-2,-1}
		pPRGBank[0] = 0;
		pPRGBank[1] = 0x2000;
		pPRGBank[2] = (uint32_t)(prgBanks * 2 - 2) * 0x2000;
		pPRGBank[3] = (uint32_t)(prgBanks * 2 - 1) * 0x2000;
	}

	bool cpuMapRead(uint16_t addr, uint32_t& mapped, uint8_t&) override {
		if (addr < 0x8000) return false;
		if (addr < 0xA000) { mapped = pPRGBank[0] + (addr & 0x1FFF); return true; }
		if (addr < 0xC000) { mapped = pPRGBank[1] + (addr & 0x1FFF); return true; }
		if (addr < 0xE000) { mapped = pPRGBank[2] + (addr & 0x1FFF); return true; }
		mapped = pPRGBank[3] + (addr & 0x1FFF); return true;
	}

	void cpuMapWrite(uint16_t addr, uint32_t&, uint8_t data) override {
		if (addr < 0x8000) return;

		if (addr < 0xA000) {
			// $8000-$9FFF
			if ((addr & 1) == 0) {
				// Bank select
				targetReg     = data & 0x07;
				prgMode       = (data & 0x40) != 0;
				chrInversion  = (data & 0x80) != 0;
			} else {
				// Bank data
				pRegister[targetReg] = data;
				updateBanks();
			}
		} else if (addr < 0xC000) {
			// $A000-$BFFF
			if ((addr & 1) == 0) {
				mirrorMode = (data & 0x01) ? Mirroring::HORIZONTAL : Mirroring::VERTICAL;
			} else {
				// PRG-RAM protect � ignorujemy
			}
		} else if (addr < 0xE000) {
			// $C000-$DFFF
			if ((addr & 1) == 0) {
				irqReload = data;
			} else {
				irqReloadFlag = true;
				irqCounter = 0;
			}
		} else {
			// $E000-$FFFF
			if ((addr & 1) == 0) {
				irqEnable = false;
				irqActive = false;
			} else {
				irqEnable = true;
			}
		}

	}

	bool ppuMapRead(uint16_t addr, uint32_t& mapped) override {
		if (addr > 0x1FFF) return false;
		if      (addr < 0x0400) mapped = pCHRBank[0] + (addr & 0x03FF);
		else if (addr < 0x0800) mapped = pCHRBank[1] + (addr & 0x03FF);
		else if (addr < 0x0C00) mapped = pCHRBank[2] + (addr & 0x03FF);
		else if (addr < 0x1000) mapped = pCHRBank[3] + (addr & 0x03FF);
		else if (addr < 0x1400) mapped = pCHRBank[4] + (addr & 0x03FF);
		else if (addr < 0x1800) mapped = pCHRBank[5] + (addr & 0x03FF);
		else if (addr < 0x1C00) mapped = pCHRBank[6] + (addr & 0x03FF);
		else                    mapped = pCHRBank[7] + (addr & 0x03FF);
		return true;
	}
	bool ppuMapWrite(uint16_t addr, uint32_t& mapped) override {
		return mapper_helpers::chrRamWrite(addr, mapped, chrBanks);
	}

	Mirroring mirror() const override { return mirrorMode; }
	bool hasDynamicMirror() const override { return true; }

	// Klasyczna scanline-based zliczanka (zachowana dla kompatybilno�ci wstecznej).
	void scanline() override {
		if (irqCounter == 0 || irqReloadFlag) {
			irqCounter = irqReload;
			irqReloadFlag = false;
		} else {
			irqCounter--;
		}
		if (irqCounter == 0 && irqEnable) {
			irqActive = true;
		}
	}

	// Precyzyjny licznik MMC3: dekrementuje przy ka�dym rosn�cym zboczu A12
	// generowanym przez PPU (typowo raz na lini�, gdy sprite'y u�ywaj� drugiej
	// tablicy pattern�w albo gdy t�o i sprite'y u�ywaj� r�nych tablic).
	void clockA12(uint16_t addr) override {
		const bool a12High = (addr & 0x1000) != 0;
		if (a12High && !lastA12) {
			if (irqCounter == 0 || irqReloadFlag) {
				irqCounter = irqReload;
				irqReloadFlag = false;
			} else {
				irqCounter--;
			}
			if (irqCounter == 0 && irqEnable) {
				irqActive = true;
			}
		}
		lastA12 = a12High;
	}

	bool irqState() const override { return irqActive; }
	void irqClear() override { irqActive = false; }

protected:
	void updateBanks() {
		if (chrInversion) {
			pCHRBank[0] = (uint32_t)pRegister[2] * 0x0400;
			pCHRBank[1] = (uint32_t)pRegister[3] * 0x0400;
			pCHRBank[2] = (uint32_t)pRegister[4] * 0x0400;
			pCHRBank[3] = (uint32_t)pRegister[5] * 0x0400;
			pCHRBank[4] = (uint32_t)(pRegister[0] & 0xFE) * 0x0400;
			pCHRBank[5] = pCHRBank[4] + 0x0400;
			pCHRBank[6] = (uint32_t)(pRegister[1] & 0xFE) * 0x0400;
			pCHRBank[7] = pCHRBank[6] + 0x0400;
		} else {
			pCHRBank[0] = (uint32_t)(pRegister[0] & 0xFE) * 0x0400;
			pCHRBank[1] = pCHRBank[0] + 0x0400;
			pCHRBank[2] = (uint32_t)(pRegister[1] & 0xFE) * 0x0400;
			pCHRBank[3] = pCHRBank[2] + 0x0400;
			pCHRBank[4] = (uint32_t)pRegister[2] * 0x0400;
			pCHRBank[5] = (uint32_t)pRegister[3] * 0x0400;
			pCHRBank[6] = (uint32_t)pRegister[4] * 0x0400;
			pCHRBank[7] = (uint32_t)pRegister[5] * 0x0400;
		}

		if (prgMode) {
			pPRGBank[0] = (uint32_t)(prgBanks * 2 - 2) * 0x2000;
			pPRGBank[2] = (uint32_t)(pRegister[6] & 0x3F) * 0x2000;
		} else {
			pPRGBank[0] = (uint32_t)(pRegister[6] & 0x3F) * 0x2000;
			pPRGBank[2] = (uint32_t)(prgBanks * 2 - 2) * 0x2000;
		}
		pPRGBank[1] = (uint32_t)(pRegister[7] & 0x3F) * 0x2000;
		pPRGBank[3] = (uint32_t)(prgBanks * 2 - 1) * 0x2000;
	}

	std::array<uint8_t, 8>  pRegister{};
	std::array<uint32_t, 8> pCHRBank{};
	std::array<uint32_t, 4> pPRGBank{};
	uint8_t targetReg = 0;
	bool prgMode = false;
	bool chrInversion = false;
	Mirroring mirrorMode = Mirroring::HORIZONTAL;

	bool irqActive = false, irqEnable = false, irqReloadFlag = false;
	bool lastA12 = false;
	uint16_t irqCounter = 0, irqReload = 0;
};

REGISTER_MAPPER(4, Mapper004)
