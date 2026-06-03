#pragma once
#include "MapperBase.h"
#include "MapperRegistry.h"
#include <array>

// Mapper 033 - Taito TC0190 (bez IRQ; wariant z IRQ to mapper 048).
class Mapper033 : public Mapper {
public:
	using Mapper::Mapper;
	void reset() override {
		prg0 = prg1 = 0;
		chr2k[0] = chr2k[1] = 0;
		for (auto& c : chr1k) c = 0;
		mirrorMode = Mirroring::VERTICAL;
	}

	bool cpuMapRead(uint16_t a, uint32_t& mapped, uint8_t&) override {
		if (a < 0x8000) return false;
		uint32_t total8k = (uint32_t)prgBanks * 2;
		if      (a < 0xA000) mapped = (uint32_t)prg0 * 0x2000 + (a & 0x1FFF);
		else if (a < 0xC000) mapped = (uint32_t)prg1 * 0x2000 + (a & 0x1FFF);
		else if (a < 0xE000) mapped = (total8k - 2) * 0x2000 + (a & 0x1FFF);
		else                 mapped = (total8k - 1) * 0x2000 + (a & 0x1FFF);
		return true;
	}
	void cpuMapWrite(uint16_t a, uint32_t&, uint8_t data) override {
		if (a < 0x8000 || a > 0xBFFF) return;
		switch (a & 0xA003) {
			case 0x8000:
				prg0 = data & 0x3F;
				mirrorMode = (data & 0x40) ? Mirroring::HORIZONTAL : Mirroring::VERTICAL;
				break;
			case 0x8001: prg1 = data & 0x3F; break;
			case 0x8002: chr2k[0] = data; break;
			case 0x8003: chr2k[1] = data; break;
			case 0xA000: chr1k[0] = data; break;
			case 0xA001: chr1k[1] = data; break;
			case 0xA002: chr1k[2] = data; break;
			case 0xA003: chr1k[3] = data; break;
		}

	}
	bool ppuMapRead(uint16_t a, uint32_t& mapped) override {
		if (a > 0x1FFF) return false;
		if (a < 0x0800)      mapped = (uint32_t)chr2k[0] * 0x0800 + (a & 0x07FF);
		else if (a < 0x1000) mapped = (uint32_t)chr2k[1] * 0x0800 + (a & 0x07FF);
		else {
			uint8_t idx = (uint8_t)((a - 0x1000) >> 10);
			mapped = (uint32_t)chr1k[idx] * 0x0400 + (a & 0x03FF);
		}
		return true;
	}
	bool ppuMapWrite(uint16_t a, uint32_t& mapped) override {
		return mapper_helpers::chrRamWrite(a, mapped, chrBanks);
	}
	Mirroring mirror() const override { return mirrorMode; }
	bool hasDynamicMirror() const override { return true; }
private:
	uint8_t prg0 = 0, prg1 = 0;
	std::array<uint8_t, 2> chr2k{};
	std::array<uint8_t, 4> chr1k{};
	Mirroring mirrorMode = Mirroring::VERTICAL;
};

REGISTER_MAPPER(33, Mapper033)
