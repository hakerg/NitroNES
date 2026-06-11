#pragma once
#include "MapperBase.h"
#include "MapperRegistry.h"

// Mapper 015 - 100-in-1 Contra Function 16. Tylko PRG bank, brak CHR-ROM.
class Mapper015 : public Mapper {
public:
	using Mapper::Mapper;
	void reset() override { reg = 0; addr = 0x8000; }

	bool cpuMapRead(uint16_t a, uint32_t& mapped, uint8_t&) override {
		if (a < 0x8000) return false;
		uint8_t mode = (uint8_t)(addrLow & 0x03);
		uint8_t P    = (reg >> 0) & 0x3F;
		uint8_t p    = (reg >> 7) & 0x01;
		uint8_t bank16k = 0; // index 16KB banków
		uint16_t off = a & 0x3FFF;
		bool xor1 = false;
		switch (mode) {
			case 0: // dwa banki 16KB: P, P|1
				if (a < 0xC000) bank16k = P;
				else            bank16k = P | 1;
				break;
			case 1: // P, ostatni
				if (a < 0xC000) bank16k = P;
				else            bank16k = (uint8_t)(prgBanks - 1);
				break;
			case 2: { // 4x 8KB tego samego banku
				uint8_t bank8k = (uint8_t)((P << 1) | p);
				mapped = (uint32_t)bank8k * 0x2000 + (a & 0x1FFF);
				return true;
			}
			case 3: // mirror 16K (P,P)
				bank16k = P;
				break;
		}
		// Mode 0: dodatkowo XOR z bitem mirroringu z A0 strony - pominięte (rzadko używane).
		(void)xor1;
		mapped = (uint32_t)bank16k * 0x4000 + off;
		return true;
	}
	void cpuMapWrite(uint16_t a, uint32_t&, uint8_t data) override {
		if (a < 0x8000) return;
		reg = data;
		addrLow = (uint8_t)(a & 0x03);
		mirrorMode = (data & 0x40) ? Mirroring::HORIZONTAL : Mirroring::VERTICAL;

	}
	bool ppuMapRead(uint16_t a, uint32_t& mapped) override {
		if (a > 0x1FFF) return false;
		mapped = a; return true;
	}
	bool ppuMapWrite(uint16_t a, uint32_t& mapped) override {
		if (a > 0x1FFF || chrBanks != 0) return false;
		mapped = a; return true;
	}
	Mirroring mirror() const override { return mirrorMode; }
	bool hasDynamicMirror() const override { return true; }
private:
	uint8_t reg = 0, addrLow = 0;
	uint16_t addr = 0x8000;
	Mirroring mirrorMode = Mirroring::HORIZONTAL;
};

REGISTER_MAPPER(15, Mapper015)
