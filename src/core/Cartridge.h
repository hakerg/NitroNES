#pragma once

#include <array>
#include <cstring>
#include <string>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>

#include "NESConst.h"
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

        const bool nes2 = (header.mapper2 & 0x0C) == 0x08;
        prgBanks = header.prg_rom_chunks;
        if (nes2)
            prgBanks |= (uint16_t)(header.tv_system1 >> 4) << 8;
        chrBanks = header.chr_rom_chunks;
        if (nes2)
            chrBanks |= (uint16_t)(header.tv_system1 & 0x0F) << 8;

        prgRomSize = prgBanks * NES::PRG_BANK_SIZE;
        if (prgRomSize > NES::MAX_PRG_ROM_SIZE)
            throw std::runtime_error("[Cartridge] PRG ROM too large: " + std::to_string(prgRomSize));
        ifs.read((char*)vPRGMemory.data(), prgRomSize);

        chrRomSize = chrBanks == 0 ? NES::CHR_BANK_SIZE : chrBanks * NES::CHR_BANK_SIZE;
        if (chrBanks == 0 && nes2) {
            uint8_t chrRamShift = (uint8_t)header.unused[0] >> 4;
            if (chrRamShift > 0)
                chrRomSize = 64 << chrRamShift;
        }
        if (chrRomSize > NES::MAX_CHR_ROM_SIZE)
            throw std::runtime_error("[Cartridge] CHR too large: " + std::to_string(chrRomSize));
        if (chrBanks != 0) ifs.read((char*)vCHRMemory.data(), chrRomSize);

        prgRamSize = 8192;
        if (nes2) {
            uint32_t size = 0;
            uint8_t nvNibble = header.prg_ram_size >> 4;
            if (nvNibble > 0)
                size = 64U << nvNibble;
            else if ((header.tv_system2 >> 4) > 0)
                size = 64U << (header.tv_system2 >> 4);
            uint8_t vNibble = header.prg_ram_size & 0x0F;
            if (vNibble > 0) {
                uint32_t vsize = 64U << vNibble;
                if (vsize > size) size = vsize;
            } else if ((header.tv_system2 & 0x0F) > 0) {
                uint32_t vsize = 64U << (header.tv_system2 & 0x0F);
                if (vsize > size) size = vsize;
            }
            if (size > 8192)
                prgRamSize = size;
        }
        if (prgRamSize > NES::MAX_PRG_RAM_SIZE)
            throw std::runtime_error("[Cartridge] PRG RAM too large: " + std::to_string(prgRamSize));
        std::memset(vPRGRAM.data(), 0, prgRamSize);

        pMapper = MapperRegistry::instance().create(mapperID, prgBanks, chrBanks);
        if (!pMapper)
            throw std::runtime_error("[Cartridge] unsupported iNES mapper #"
                                     + std::to_string((int)mapperID));

        pMapper->setChrRam1kBanks((uint16_t)(chrRomSize / 1024));
        pMapper->setAudioSettings(audioSettings);
    }

    Mirroring getMirroring() const {
        if (pMapper->hasDynamicMirror()) return pMapper->mirror();
        return hwMirror;
    }

    uint16_t getMapperID() const { return mapperID; }
    const Mapper& getMapper() const { return *pMapper; }

    uint16_t getPrgBanks() const { return prgBanks; }
    uint16_t getChrBanks() const { return chrBanks; }
    bool hasPrgRam() const { return pMapper->hasPrgRam(); }
    bool hasBusConflicts() const { return pMapper->hasBusConflicts(); }

    void reset() { pMapper->reset(); }

    bool irqState() const { return pMapper->irqState(); }
    void irqClear()       { pMapper->irqClear(); }
    void ppuAddress(uint16_t addr) { pMapper->ppuAddress(addr); }
    void clockPpu()        { pMapper->clockPpu(); }
    void clock()          { pMapper->clock(); }

    float audioOutput() const { return pMapper->audioOutput(); }

    uint8_t cpuRead(uint16_t addr, uint8_t openBusFallback = 0x00) {
        uint32_t mapped = 0;
        uint8_t data = openBusFallback;

        if (pMapper->cpuReadDirect(addr, data)) return data;

        if (pMapper->hasPrgRam() && addr >= 0x6000 && addr <= 0x7FFF) {
            return vPRGRAM[addr & 0x1FFF];
        }

        if (pMapper->cpuMapRead(addr, mapped, data)) {
            if (mapped < prgRomSize) return vPRGMemory[mapped];
            return data;
        }
        return openBusFallback;
    }

    void cpuWrite(uint16_t addr, uint8_t data) {
        uint32_t mapped = 0;

        if (pMapper->cpuWriteDirect(addr, data)) return;

        if (pMapper->hasPrgRam() && addr >= 0x6000 && addr <= 0x7FFF) {
            vPRGRAM[addr & 0x1FFF] = data;
            return;
        }

        if (pMapper->hasBusConflicts() && addr >= 0x8000) {
            uint32_t romOff = 0;
            uint8_t dummy = 0;
            if (pMapper->cpuMapRead(addr, romOff, dummy) && romOff < prgRomSize) {
                data &= vPRGMemory[romOff];
            }
        }
        pMapper->cpuMapWrite(addr, mapped, data);
    }

    bool ppuRead(uint16_t addr, uint8_t& data) {
        uint32_t mapped = 0;
        pMapper->ppuReadCycle(addr);
        if (pMapper->ppuReadDirect(addr, data)) return true;
        if (pMapper->ppuMapRead(addr, mapped)) {
            if (mapped < chrRomSize) data = vCHRMemory[mapped];
            return true;
        }
        return false;
    }

    bool ppuWrite(uint16_t addr, uint8_t data) {
        uint32_t mapped = 0;
        if (pMapper->ppuWriteDirect(addr, data)) return true;
        if (pMapper->ppuMapWrite(addr, mapped)) {
            if (mapped < chrRomSize) vCHRMemory[mapped] = data;
            return true;
        }
        return false;
    }

private:
    uint8_t mapperID = 0;
    uint16_t prgBanks = 0;
    uint16_t chrBanks = 0;
    Mirroring hwMirror = Mirroring::HORIZONTAL;

    std::array<uint8_t, NES::MAX_PRG_ROM_SIZE> vPRGMemory{};
    std::array<uint8_t, NES::MAX_CHR_ROM_SIZE> vCHRMemory{};
    std::array<uint8_t, NES::MAX_PRG_RAM_SIZE> vPRGRAM{};
    size_t prgRomSize = 0;
    size_t chrRomSize = 0;
    size_t prgRamSize = 0;

    std::unique_ptr<Mapper> pMapper;
};
