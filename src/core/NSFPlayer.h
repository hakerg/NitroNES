#pragma once
#include <array>
#include <vector>
#include <string>
#include <memory>
#include <fstream>
#include <cstring>
#include <iostream>
#include <filesystem>
#include <span>
#include <spanstream>
#include "NESConst.h"
#include "NESCoreBase.h"
#include "mappers/MapperBase.h"
#include "mappers/MapperRegistry.h"

#pragma pack(push, 1)
struct NSFHeader {
    char     magic[5] = {};         // "NESM\x1A"
    uint8_t  version = 0;          // Wersja (0x01)
    uint8_t  totalSongs = 0;       // Liczba utworów (1-based)
    uint8_t  startingSong = 0;     // Utwór startowy (1-based)
    uint16_t loadAddr = 0;         // Adres załadowania danych (0x8000-0xFFFF)
    uint16_t initAddr = 0;         // Adres rutyny INIT
    uint16_t playAddr = 0;         // Adres rutyny PLAY
    char     songName[32] = {};     // Tytuł (null-terminated)
    char     artist[32] = {};       // Artysta (null-terminated)
    char     copyright[32] = {};    // Prawa autorskie (null-terminated)
    uint16_t speedNTSC = 0;        // Szybkość odtwarzania NTSC (1/1000000 s)
    uint8_t  bankValues[8] = {};    // Wartości bankswitch (0 = brak bankswitch)
    uint16_t speedPAL = 0;         // Szybkość odtwarzania PAL (1/1000000 s)
    uint8_t  palNtscBits = 0;      // bit0: PAL, bit1: dual
    uint8_t  extraChipFlags = 0;   // Dodatkowe układy dźwiękowe
    uint8_t  reserved[4] = {};      // Zarezerwowane (0x00)

    bool isBankswitched() const {
        return std::any_of(std::begin(bankValues), std::end(bankValues), [](uint8_t bv) { return bv != 0; });
    }

    bool isPAL()      const { return (palNtscBits & 0x01) != 0; }
    bool isDualMode() const { return (palNtscBits & 0x02) != 0; }
    std::string title()         const { return safeStr(songName); }
    std::string artistName()    const { return safeStr(artist); }
    std::string copyrightText() const { return safeStr(copyright); }

private:
    static std::string safeStr(const char* buf) {
        int len = 0;
        while (len < 32 && buf[len] != '\0') len++;
        return std::string(buf, len);
    }
};
#pragma pack(pop)

static_assert(sizeof(NSFHeader) == 0x80, "NSFHeader musi mieć 128 bajtów");

struct NSFFile {
    NSFHeader header;
    std::vector<uint8_t> data;  // Dane muzyczne (od offsetu 0x80)
};

class NSFPlayer : public NESCoreBase {
public:
    static constexpr uint16_t TRAMPOLINE_ADDR   = 0x5000;
    static constexpr uint16_t RESET_VECTOR      = 0xFFFC;
    static constexpr int      CALL_WATCHDOG_CYCLES = 200000;

    explicit NSFPlayer(AudioSettings& audioSettings, const std::string& path) {
        auto& bus = NESBus::instance();
        bus.apu = &a2a03.getAPU();
        bus.core = this;
        bus.cpuBus = &a2a03;
        bus.dmaBus = &a2a03;
        bus.audio = &audioSettings;
        bus.cart = nullptr;
        bus.ppu = nullptr;
        extRam.fill(0x00);
        prgRom.assign(32768, 0x00);
        NSFFile nsf;
        if (!loadFromFile(path, nsf))
            throw std::runtime_error("[NSF] Nie udalo sie zaladowac: " + path);
        load(nsf, audioSettings);
    }

    static bool loadFromFile(const std::string& path, NSFFile& out) {
        std::ifstream f(std::filesystem::path(path), std::ios::binary);
        if (!f.is_open()) {
            std::cerr << "[NSF] Nie można otworzyć pliku: " << path << "\n";
            return false;
        }

        f.read(reinterpret_cast<char*>(&out.header), sizeof(NSFHeader));
        if (f.gcount() < (std::streamsize)sizeof(NSFHeader)) {
            std::cerr << "[NSF] Plik za krótki (niepełny nagłówek)\n";
            return false;
        }

        if (std::strncmp(out.header.magic, "NESM\x1A", 5) != 0) {
            std::cerr << "[NSF] Nieprawidłowa sygnatura. Oczekiwano: 4E 45 53 4D 1A, odczytano: ";
            for (char i : out.header.magic)
                std::cerr << std::hex << std::uppercase
                          << ((unsigned)(unsigned char)i) << " ";
            std::cerr << std::dec << "\n";
            std::cerr << "[NSF] Ścieżka: " << path << "\n";
            return false;
        }

        out.data.assign(std::istreambuf_iterator<char>(f),
                        std::istreambuf_iterator<char>());
        return true;
    }

    bool load(const NSFFile& nsf, AudioSettings& audioSettings) {
        nsfHeader = nsf.header;
        applySystem();

        expChip.reset();
        uint8_t chips = nsfHeader.extraChipFlags;
        if (chips & 0x01) {
            expChip = MapperRegistry::instance().create(24, 1, 1);
            expChip->setAudioSettings(audioSettings);
        }

        if (nsfHeader.isBankswitched()) {
            loadBankswitched(nsf.data);
        } else {
            uint16_t base   = nsfHeader.loadAddr;
            size_t   maxLen = prgRom.size() - (base - 0x8000);
            size_t   len    = std::min(nsf.data.size(), maxLen);
            std::fill(prgRom.begin(), prgRom.end(), 0x00);
            std::copy(nsf.data.begin(), nsf.data.begin() + len,
                    prgRom.begin() + (base - 0x8000));
        }

        trampoline[0] = 0x4C;
        trampoline[1] = TRAMPOLINE_ADDR & 0xFF;
        trampoline[2] = (TRAMPOLINE_ADDR >> 8) & 0xFF;

        currentSong = nsfHeader.startingSong;
        return true;
    }

    void initSong(uint8_t songNum) {
        currentSong = songNum;
        playTimer   = 0;
        callDone    = true;

        cpuRam.fill(0x00);
        extRam.fill(0x00);

        for (uint16_t a = 0x4000; a <= 0x400F; a++) a2a03.getAPU().writeData(a, 0x00, false);
        a2a03.getAPU().writeData(0x4010, 0x10, false);
        a2a03.getAPU().writeData(0x4011, 0x00, false);
        a2a03.getAPU().writeData(0x4012, 0x00, false);
        a2a03.getAPU().writeData(0x4013, 0x00, false);
        a2a03.getAPU().writeData(0x4015, 0x0F, false);
        a2a03.getAPU().writeData(0x4017, 0x40, false);

        if (nsfHeader.isBankswitched()) {
            for (int i = 0; i < 8; i++)
                banks[i] = nsfHeader.bankValues[i];
        }

        cpuRam[RESET_VECTOR & 0x07FF] = 0x00;

        a2a03.getCPU().reset();
        a2a03.getCPU().A = songNum - 1;
        a2a03.getCPU().X = usePal() ? 1 : 0;
        a2a03.getCPU().P = CPU6502::FLAG_I | CPU6502::FLAG_U;
        a2a03.getCPU().S = 0xFD;

        pushWord(TRAMPOLINE_ADDR - 1);
        a2a03.getCPU().jumpTo(nsfHeader.initAddr);

        int initCycles = 0;
        for (; initCycles < CALL_WATCHDOG_CYCLES && !isAtTrampoline(); initCycles++) {
            a2a03.clockPhi1();
            a2a03.clockPhi2Write();
            a2a03.clockPhi2();
        }
    }

    void nextSong() {
        uint8_t next = currentSong + 1;
        if (next > nsfHeader.totalSongs) next = 1;
        initSong(next);
    }

    void prevSong() {
        uint8_t prev = currentSong - 1;
        if (prev < 1) prev = nsfHeader.totalSongs;
        initSong(prev);
    }

    uint8_t getCurrentSong()  const { return currentSong; }
    uint8_t getTotalSongs()   const { return nsfHeader.totalSongs; }

    const NSFHeader& header() const { return nsfHeader; }

    double getBaseFramerate() const override {
        return cpuClock / playCycles;
    }

    void reset() override { initSong(currentSong); }
    bool pollNMI()     override { return true; }
    bool irqAsserted() override { return false; }
    uint32_t* getFramebuffer() override { return nullptr; }
    int getCompletedFramesCount() override { return completedFramesCount; }
    int getCurrentScanline() override { return -1; }
    int getTotalScanlines()  override { return -1; }

    std::vector<uint8_t> saveState() const override {
        size_t sz = a2a03.stateSize() + cpuRam.size() + sizeof(system);
        std::vector<uint8_t> buf(sz);
        std::ospanstream ss(std::span(reinterpret_cast<char*>(buf.data()), sz));
        a2a03.save(ss);
        ss.write(reinterpret_cast<const char*>(cpuRam.data()), cpuRam.size());
        ss.write(reinterpret_cast<const char*>(&system), sizeof(system));
        return buf;
    }
    void loadState(const std::vector<uint8_t>& data) override {
        if (data.size() < cpuRam.size()) return;
        std::ispanstream ss(std::span(reinterpret_cast<const char*>(data.data()), data.size()));
        a2a03.load(ss);
        ss.read(reinterpret_cast<char*>(cpuRam.data()), cpuRam.size());
        ss.read(reinterpret_cast<char*>(&system), sizeof(system));
    }

    uint8_t peekMemory(uint16_t addr) override {
        if (addr <= 0x07FF) return cpuRam[addr];
        if (addr == 0x4015) return a2a03.getAPU().cpuPeek(addr);
        if (addr == 0x4016 || addr == 0x4017) return 0x00;
        if (addr >= 0x2000 && addr <= 0x3FFF) return 0x80;
        if (addr >= 0x5000 && addr <= 0x5002) return trampoline[addr - 0x5000];
        if (addr >= 0x6000 && addr <= 0x7FFF) return extRam[addr - 0x6000];
        if (addr >= 0x8000) {
            if (nsfHeader.isBankswitched()) {
                uint8_t bankIdx = (addr - 0x8000) / 4096;
                uint16_t offset = (addr - 0x8000) % 4096;
                uint32_t romAddr = (uint32_t)banks[bankIdx] * 4096 + offset;
                return (romAddr < bankRom.size()) ? bankRom[romAddr] : 0x00;
            }
            return prgRom[addr - 0x8000];
        }
        return 0x00;
    }

protected:
    void clockOneCycle() override {
        trampolineMaintenance();
        playTimer -= 1.0;
        if (!callDone) {
            if (isAtTrampoline()) callDone = true;
            else if (playTimer < -CALL_WATCHDOG_CYCLES) triggerPlayCall();
        }

        if (expChip) expChip->clock();
        a2a03.clockPhi1();
        a2a03.clockPhi2Write();
        a2a03.clockPhi2();

        pushAudioOutput(expChip ? expChip->audioOutput() : 0.0f);
    }

    void applySystem() override {
        // NSF zna tylko NTSC/PAL; Dendy ma APU z timingiem NTSC
        const bool pal = usePal();
        a2a03.setRegion(pal ? NESStandard::PAL
                            : (system == NESStandard::DENDY ? NESStandard::DENDY
                                                            : NESStandard::NTSC));
        cpuClock = pal ? NES::CPU_CLOCK_PAL : getCPUClockRate();
        uint16_t speed = pal ? nsfHeader.speedPAL : nsfHeader.speedNTSC;
        if (speed == 0) speed = pal ? NES::NSF_SPEED_PAL : NES::NSF_SPEED_NTSC;
        playCycles = cpuClock * speed / 1000000.0;
    }

    uint8_t readMemoryExternal(uint16_t addr) override { return readMemory(addr); }

    uint8_t readMemory(uint16_t addr) override {
        uint8_t data = a2a03.getBusData();
        if (addr <= 0x07FF) {
            data = cpuRam[addr];
        }
        else if (addr >= 0x2000 && addr <= 0x3FFF) {
            data = 0x80;
        }
        else if (addr >= 0x5000 && addr <= 0x5002) {
            data = trampoline[addr - 0x5000];
        }
        else if (addr >= 0x6000 && addr <= 0x7FFF) {
            data = extRam[addr - 0x6000];
        }
        else if (addr >= 0x8000) {
            if (nsfHeader.isBankswitched()) {
                uint8_t  bankIdx = (addr - 0x8000) / 4096;
                uint16_t offset  = (addr - 0x8000) % 4096;
                uint32_t romAddr = (uint32_t)banks[bankIdx] * 4096 + offset;
                data = (romAddr < bankRom.size()) ? bankRom[romAddr] : 0x00;
            } else {
                data = prgRom[addr - 0x8000];
            }
        }
        return data;
    }

    void writeMemoryMapped(uint16_t addr, uint8_t data) override {
        if (addr <= 0x07FF) { cpuRam[addr] = data; return; }
        if (addr >= 0x2000 && addr <= 0x3FFF) return;
        if (addr >= 0x6000 && addr <= 0x7FFF) { extRam[addr - 0x6000] = data; return; }
        if (addr >= 0x5FF8 && addr <= 0x5FFF) {
            banks[addr - 0x5FF8] = data;
            return;
        }
        if (expChip && addr >= 0x8000) {
            uint32_t dummy = 0;
            expChip->cpuMapWrite(addr, dummy, data);
        }
    }

private:
    bool usePal() const {
        return nsfHeader.isPAL()
            || (nsfHeader.isDualMode() && system == NESStandard::PAL);
    }

    bool isAtTrampoline() {
        const uint16_t pc = a2a03.getCPU().PC;
        return pc >= TRAMPOLINE_ADDR && pc <= TRAMPOLINE_ADDR + 3;
    }

    void trampolineMaintenance() {
        if (callDone && playTimer <= 0.0) {
            if (!a2a03.getCPU().isAtInstructionBoundary()) return;
            triggerPlayCall();
        }
    }

    void triggerPlayCall() {
        a2a03.getCPU().S = 0xFD;
        callDone = false;
        pushWord(TRAMPOLINE_ADDR - 1);
        a2a03.getCPU().jumpTo(nsfHeader.playAddr);

        playTimer += playCycles;

        completedFramesCount++;
    }



    void loadBankswitched(const std::vector<uint8_t>& data) {
        uint16_t loadOffset  = nsfHeader.loadAddr & 0x0FFF;
        size_t   romCapacity = 256 * 4096;
        bankRom.assign(romCapacity, 0x00);
        size_t len = std::min(data.size(), romCapacity - loadOffset);
        std::copy(data.begin(), data.begin() + len,
                bankRom.begin() + loadOffset);
        for (int i = 0; i < 8; i++)
            banks[i] = nsfHeader.bankValues[i];
    }

    void pushWord(uint16_t val) {
        cpuRam[0x0100 + a2a03.getCPU().S] = (val >> 8) & 0xFF;
        a2a03.getCPU().S--;
        cpuRam[0x0100 + a2a03.getCPU().S] = val & 0xFF;
        a2a03.getCPU().S--;
    }

    NSFHeader nsfHeader  = {};
    double    cpuClock    = NES::CPU_CLOCK_NTSC;
    double    playCycles    = 0.0;
    double    playTimer     = 0.0;
    bool      callDone    = true;
    uint8_t   currentSong = 1;
    int       completedFramesCount = 0;

    std::array<uint8_t, 8192>  extRam;
    std::vector<uint8_t>       prgRom;

    std::array<uint8_t, 8>   banks   = {};
    std::vector<uint8_t>     bankRom;

    uint8_t trampoline[3] = {};

    std::unique_ptr<Mapper> expChip;
};
