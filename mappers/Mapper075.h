#pragma once
#include "MapperBase.h"
#include "MapperRegistry.h"
#include <array>

// Mapper 075 - VRC1.
class Mapper075 : public Mapper {
public:
	using Mapper::Mapper;
	void reset() override {
		prg[0] = 0; prg[1] = 0; prg[2] = 0;
		chrHi = 0; chrLo[0] = 0; chrLo[1] = 0;
		mirrorMode = Mirroring::VERTICAL;
	}
	bool cpuMapRead(uint16_t a, uint32_t& mapped, uint8_t&) override {
		if (a < 0x8000) return false;
		uint32_t total8k = (uint32_t)prgBanks * 2;
		uint8_t b;
		if      (a < 0xA000) b = prg[0];
		else if (a < 0xC000) b = prg[1];
		else if (a < 0xE000) b = prg[2];
		else                 b = (uint8_t)(total8k - 1);
		mapped = (uint32_t)b * 0x2000 + (a & 0x1FFF);
		return true;
	}
	void cpuMapWrite(uint16_t a, uint32_t&, uint8_t data) override {
		if (a < 0x8000) return;
		switch (a & 0xF000) {
			case 0x8000: prg[0] = data & 0x0F; break;
			case 0xA000: prg[1] = data & 0x0F; break;
			case 0xC000: prg[2] = data & 0x0F; break;
			case 0x9000:
				mirrorMode = (data & 0x01) ? Mirroring::HORIZONTAL : Mirroring::VERTICAL;
				chrHi = data & 0x06;
				break;
			case 0xE000: chrLo[0] = data & 0x0F; break;
			case 0xF000: chrLo[1] = data & 0x0F; break;
		}

	}
	bool ppuMapRead(uint16_t a, uint32_t& mapped) override {
		if (a > 0x1FFF) return false;
		if (a < 0x1000) {
			uint8_t hi = (chrHi & 0x02) >> 1;
			uint8_t b = (uint8_t)((hi << 4) | chrLo[0]);
			mapped = (uint32_t)b * 0x1000 + (a & 0x0FFF);
		} else {
			uint8_t hi = (chrHi & 0x04) >> 2;
			uint8_t b = (uint8_t)((hi << 4) | chrLo[1]);
			mapped = (uint32_t)b * 0x1000 + (a & 0x0FFF);
		}
		return true;
	}
	bool ppuMapWrite(uint16_t a, uint32_t& mapped) override {
		return mapper_helpers::chrRamWrite(a, mapped, chrBanks);
	}
	Mirroring mirror() const override { return mirrorMode; }
	bool hasDynamicMirror() const override { return true; }
private:
	std::array<uint8_t, 3> prg{};
	std::array<uint8_t, 2> chrLo{};
	uint8_t chrHi = 0;
	Mirroring mirrorMode = Mirroring::VERTICAL;
};

REGISTER_MAPPER(75, Mapper075)
