#pragma once
#include <cstdint>
#include <array>
#include "Cartridge.h"

// Fizyczna szyna danych i adresowa układu graficznego (PPU Bus / VRAM Bus)
class PPUBus {
public:
    PPUBus() {
        nameTable[0].fill(0x00);
        nameTable[1].fill(0x00);
    }

    Cartridge* cart = nullptr;

    // 2KB sprzętowego VRAMu na płycie głównej NESa na Tablice Nazw (Nametables)
    std::array<std::array<uint8_t, 1024>, 2> nameTable;

    void write(uint16_t addr, uint8_t data) {
        addr &= 0x3FFF; // Przestrzeń PPU ma 14 bitów

        if (cart && cart->ppuWrite(addr, data)) {
            // Kartridż przyjął zapis (np. posiada układ CHR-RAM)
        }
        else if (addr >= 0x2000 && addr <= 0x3EFF) {
            // VRAM konsoli
            addr &= 0x0FFF;

            // Mirroring (zarządzany przez Mappera w kartridżu)
            Mirroring m = cart ? cart->getMirroring() : Mirroring::HORIZONTAL;
            if (m == Mirroring::VERTICAL) {
                nameTable[(addr & 0x0400) >> 10][addr & 0x03FF] = data;
            }
            else if (m == Mirroring::HORIZONTAL) {
                nameTable[(addr & 0x0800) >> 11][addr & 0x03FF] = data;
            }
            else if (m == Mirroring::ONESCREEN_LO) {
                nameTable[0][addr & 0x03FF] = data;
            }
            else if (m == Mirroring::ONESCREEN_HI) {
                nameTable[1][addr & 0x03FF] = data;
            }
        }
    }

    uint8_t read(uint16_t addr, bool bReadOnly = false) {
        uint8_t data = 0x00;
        addr &= 0x3FFF;

        if (cart && cart->ppuRead(addr, data)) {
            // Kartridż zaspokoił odczyt z CHR (najpewniej tablica wzorców kafelków)
        }
        else if (addr >= 0x2000 && addr <= 0x3EFF) {
            addr &= 0x0FFF;

            if (cart && cart->getMirroring() == Mirroring::VERTICAL) {
                data = nameTable[(addr & 0x0400) >> 10][addr & 0x03FF];
            }
            else if (cart && cart->getMirroring() == Mirroring::HORIZONTAL) {
                data = nameTable[(addr & 0x0800) >> 11][addr & 0x03FF];
            }
            else if (cart && cart->getMirroring() == Mirroring::ONESCREEN_LO) {
                data = nameTable[0][addr & 0x03FF];
            }
            else if (cart && cart->getMirroring() == Mirroring::ONESCREEN_HI) {
                data = nameTable[1][addr & 0x03FF];
            }
        }

        return data;
    }
};
