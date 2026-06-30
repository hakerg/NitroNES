#pragma once

#include <vector>
#include <string>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>

#include "mappers/MapperBase.h"
#include "mappers/MapperRegistry.h"
#include "mappers/AllMappers.h"

class Cartridge {
public:
    Cartridge(const std::string& sFileName, AudioSettings& audioSettings) {
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

        std::ifstream ifs(sFileName, std::ifstream::binary);
        if (!ifs.is_open())
            throw std::runtime_error("[Cartridge] cannot open: " + sFileName);

        ifs.read((char*)&header, sizeof(sHeader));
        if (!(header.name[0] == 'N' && header.name[1] == 'E' &&
              header.name[2] == 'S' && header.name[3] == 0x1A))
            throw std::runtime_error("[Cartridge] invalid iNES header: " + sFileName);

        mapperID = ((header.mapper2 >> 4) << 4) | (header.mapper1 >> 4);
        hwMirror = (header.mapper1 & 0x08)
            ? Mirroring::FOURSCREEN
            : ((header.mapper1 & 0x01) ? Mirroring::VERTICAL : Mirroring::HORIZONTAL);

        if (header.mapper1 & 0x04) ifs.seekg(512, std::ios_base::cur);

        prgBanks = header.prg_rom_chunks;
        chrBanks = header.chr_rom_chunks;

        vPRGMemory.resize(prgBanks * 16384);
        ifs.read((char*)vPRGMemory.data(), vPRGMemory.size());

        vCHRMemory.resize(chrBanks == 0 ? 8192 : chrBanks * 8192);
        if (chrBanks != 0) ifs.read((char*)vCHRMemory.data(), vCHRMemory.size());

        vPRGRAM.assign(8192, 0x00);

        pMapper = MapperRegistry::instance().create(mapperID, prgBanks, chrBanks);
        if (!pMapper)
            throw std::runtime_error("[Cartridge] unsupported iNES mapper #"
                                     + std::to_string((int)mapperID));

        pMapper->setAudioSettings(audioSettings);
    }

    Mirroring getMirroring() const {
        if (pMapper->hasDynamicMirror()) return pMapper->mirror();
        return hwMirror;
    }

    void reset() { pMapper->reset(); }

    bool irqState() const { return pMapper->irqState(); }
    void irqClear()       { pMapper->irqClear(); }
    void clockA12(uint16_t addr, uint64_t ppuCycle) { pMapper->clockA12(addr, ppuCycle); }
    void clock()          { pMapper->clock(); }

    float audioOutput() const { return pMapper->audioOutput(); }

    uint8_t cpuRead(uint16_t addr, uint8_t openBusFallback = 0x00) {
        uint32_t mapped = 0;
        uint8_t data = openBusFallback;

        if (addr >= 0x6000 && addr <= 0x7FFF) {
            return vPRGRAM[addr & 0x1FFF];
        }

        if (pMapper->cpuMapRead(addr, mapped, data)) {
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

        if (pMapper->hasBusConflicts() && addr >= 0x8000) {
            uint32_t romOff = 0;
            uint8_t dummy = 0;
            if (pMapper->cpuMapRead(addr, romOff, dummy) && romOff < vPRGMemory.size()) {
                data &= vPRGMemory[romOff];
            }
        }
        pMapper->cpuMapWrite(addr, mapped, data);
    }

    bool ppuRead(uint16_t addr, uint8_t& data) {
        uint32_t mapped = 0;
        if (pMapper->ppuMapRead(addr, mapped)) {
            if (mapped < vCHRMemory.size()) data = vCHRMemory[mapped];
            return true;
        }
        return false;
    }

    bool ppuWrite(uint16_t addr, uint8_t data) {
        uint32_t mapped = 0;
        if (pMapper->ppuMapWrite(addr, mapped)) {
            if (mapped < vCHRMemory.size()) vCHRMemory[mapped] = data;
            return true;
        }
        return false;
    }

private:
    uint8_t mapperID = 0;
    uint8_t prgBanks = 0;
    uint8_t chrBanks = 0;
    Mirroring hwMirror = Mirroring::HORIZONTAL;

    std::vector<uint8_t> vPRGMemory;
    std::vector<uint8_t> vCHRMemory;
    std::vector<uint8_t> vPRGRAM;

    std::unique_ptr<Mapper> pMapper;
};

