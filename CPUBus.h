#pragma once
#include <cstdint>
#include <array>

#include "Cartridge.h"
#include "PPU2C02.h"
#include "APU2A03.h"

// ============================================================================
// CPUBus - główna szyna procesora 2A03 (16-bit adres, 8-bit dane)
// ----------------------------------------------------------------------------
// Sprzętowo odpowiada za rozdzielanie cyklu CPU pomiędzy:
//   $0000-$1FFF  - 2KB wewnętrznego WRAM (mirror co 2KB)
//   $2000-$3FFF  - 8 rejestrów PPU (mirror co 8 bajtów)
//   $4000-$4017  - rejestry APU + I/O (kontrolery, DMA)
//   $4018-$401F  - rejestry testowe (zwykle nieużywane)
//   $4020-$FFFF  - przestrzeń kartridża (mapper, PRG-ROM, PRG-RAM)
// PPU posiada własną, odrębną szynę graficzną (zobacz PPUBus.h).
// ============================================================================
class CPUBus {
public:
	CPUBus() {
		cpuRam.fill(0x00);
		controller.fill(0x00);
		controller_state.fill(0x00);
	}

	PPU2C02* ppu = nullptr;
	APU2A03* apu = nullptr;
	Cartridge* cart = nullptr;

	std::array<uint8_t, 2048> cpuRam;
	std::array<uint8_t, 2> controller;
	std::array<uint8_t, 2> controller_state;

	inline void write(uint16_t addr, uint8_t data) {
		// Twardy, sprzętowy podział przestrzeni adresowej
		if (addr < 0x2000) {
			cpuRam[addr & 0x07FF] = data;
		}
		else if (addr < 0x4000) {
			if (ppu) ppu->cpuWrite(addr & 0x0007, data);
		}
		else if (addr < 0x4020) {
			if (addr == 0x4014) {
				if (ppu) ppu->cpuWrite(addr, data);
			}
			else if (addr == 0x4016) {
				controller_state[0] = controller[0];
				controller_state[1] = controller[1];
			}
			else if (apu) {
				apu->cpuWrite(addr, data);
			}
		}
		else {
			// Wszystko >= 0x4020 idzie bezpośrednio na piny kartridża
			if (cart) cart->cpuWrite(addr, data);
		}
	}

	inline uint8_t read(uint16_t addr, bool bReadOnly = false) {
		if (addr < 0x2000) {
			return cpuRam[addr & 0x07FF];
		}
		if (addr < 0x4000) {
			return ppu ? ppu->cpuRead(addr & 0x0007, bReadOnly) : 0x00;
		}
		if (addr < 0x4020) {
			if (addr == 0x4016) {
				uint8_t data = (controller_state[0] & 0x80) > 0 ? 1 : 0;
				if (!bReadOnly) controller_state[0] <<= 1;
				return data;
			}
			if (addr == 0x4017) {
				uint8_t data = (controller_state[1] & 0x80) > 0 ? 1 : 0;
				if (!bReadOnly) controller_state[1] <<= 1;
				return data;
			}
			// Pozostałe rejestry APU/IO ($4015 status, $4017 frame counter)
			return apu ? apu->cpuRead(addr) : 0x00;
		}

		// Strefa kartridża (PRG ROM, PRG RAM, rejestry mapperów)
		return cart ? cart->cpuRead(addr) : 0x00;
	}
};
