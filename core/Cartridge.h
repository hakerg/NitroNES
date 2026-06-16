#pragma once

#include <vector>
#include <string>
#include <fstream>
#include <iostream>
#include <memory>

#include "mappers/MapperBase.h"
#include "mappers/MapperRegistry.h"

class Cartridge {
public:
    Cartridge(const std::string& sFileName) {
        struct sHeader {
            char name[4];
            uint8_t prg_rom_chunks;
            uint8_t chr_rom_chunks;
            uint8_t mapper1;
            uint8_t mapper2;
            uint8_t prg_ram_size;
            uint8_t tv_system1;
            uint8_t tv_system2;
            char unused[5];
        } header;

        bImageValid = false;
        std::ifstream ifs;
        ifs.open(sFileName, std::ifstream::binary);

        if (ifs.is_open()) {
            ifs.read((char*)&header, sizeof(sHeader));

            if (header.name[0] == 'N' && header.name[1] == 'E' &&
                header.name[2] == 'S' && header.name[3] == 0x1A)
            {
                mapperID = ((header.mapper2 >> 4) << 4) | (header.mapper1 >> 4);

                if (header.mapper1 & 0x08) {
                    hwMirror = Mirroring::FOURSCREEN;
                } else {
                    hwMirror = (header.mapper1 & 0x01) ? Mirroring::VERTICAL : Mirroring::HORIZONTAL;
                }

                if (header.mapper1 & 0x04) {
                    ifs.seekg(512, std::ios_base::cur);
                }

                prgBanks = header.prg_rom_chunks;
                chrBanks = header.chr_rom_chunks;

                vPRGMemory.resize(prgBanks * 16384);
                ifs.read((char*)vPRGMemory.data(), vPRGMemory.size());

                if (chrBanks == 0) {
                    vCHRMemory.resize(8192);
                } else {
                    vCHRMemory.resize(chrBanks * 8192);
                    ifs.read((char*)vCHRMemory.data(), vCHRMemory.size());
                }

                vPRGRAM.assign(8192, 0x00);

                pMapper = MapperRegistry::instance().create(mapperID, prgBanks, chrBanks);
                if (!pMapper) {
                    std::cerr << "Nieobslugiwany mapper iNES #" << (int)mapperID << std::endl;
                    bImageValid = false;
                    ifs.close();
                    return;
                }

                bImageValid = true;
            }
            ifs.close();
        }
    }

    ~Cartridge() = default;

    bool isImageValid() const { return bImageValid; }

    Mirroring getMirroring() const {
        if (pMapper && pMapper->hasDynamicMirror()) return pMapper->mirror();
        return hwMirror;
    }

    void reset() {
        if (pMapper) pMapper->reset();
    }

    bool irqState() const { return pMapper && pMapper->irqState(); }
    void irqClear()       { if (pMapper) pMapper->irqClear(); }
    void scanline()       { if (pMapper) pMapper->scanline(); }
    void clockA12(bool a12High) { if (pMapper) pMapper->clockA12(a12High); }
    void clock()          { if (pMapper) pMapper->clock(); }

    float audioOutput() const { return pMapper ? pMapper->audioOutput() : 0.0f; }

    uint8_t cpuRead(uint16_t addr, uint8_t openBusFallback = 0x00) {
        uint32_t mapped = 0;
        uint8_t data = openBusFallback;

        if (addr >= 0x6000 && addr <= 0x7FFF) {
            return vPRGRAM[addr & 0x1FFF];
        }

        if (pMapper && pMapper->cpuMapRead(addr, mapped, data)) {
            if (mapped < vPRGMemory.size()) return vPRGMemory[mapped];
            return data;
        }
        return openBusFallback;
    }

    void cpuWrite(uint16_t addr, uint8_t data) {
        uint32_t mapped = 0;

        if (addr >= 0x6000 && addr <= 0x7FFF) {
            vPRGRAM[addr & 0x1FFF] = data;
            return;
        }

        if (pMapper) {
            if (pMapper->hasBusConflicts() && addr >= 0x8000) {
                uint32_t romOff = 0;
                uint8_t dummy = 0;
                if (pMapper->cpuMapRead(addr, romOff, dummy) && romOff < vPRGMemory.size()) {
                    data &= vPRGMemory[romOff];
                }
            }
            pMapper->cpuMapWrite(addr, mapped, data);
        }
    }

    bool ppuRead(uint16_t addr, uint8_t& data) {
        uint32_t mapped = 0;
        if (pMapper && pMapper->ppuMapRead(addr, mapped)) {
            if (mapped < vCHRMemory.size()) data = vCHRMemory[mapped];
            return true;
        }
        return false;
    }

    bool ppuWrite(uint16_t addr, uint8_t data) {
        uint32_t mapped = 0;
        if (pMapper && pMapper->ppuMapWrite(addr, mapped)) {
            if (mapped < vCHRMemory.size()) vCHRMemory[mapped] = data;
            return true;
        }
        return false;
    }

private:
    bool bImageValid = false;
    uint8_t mapperID = 0;
    uint8_t prgBanks = 0;
    uint8_t chrBanks = 0;
    Mirroring hwMirror = Mirroring::HORIZONTAL;

    std::vector<uint8_t> vPRGMemory;
    std::vector<uint8_t> vCHRMemory;
    std::vector<uint8_t> vPRGRAM;

    std::unique_ptr<Mapper> pMapper;
};

