#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <fstream>
#include <cstring>
#include <iostream>
#include <filesystem>

#include "NESConst.h"

// ============================================================
//  NSFLoader - wczytuje i parsuje plik NSF (NES Sound Format)
//  Spec: nsfspec.txt (Kevin Horton)
// ============================================================

#pragma pack(push, 1)
struct NSFHeader {
    char     magic[5];         // "NESM\x1A"
    uint8_t  version;          // Wersja (0x01)
    uint8_t  totalSongs;       // Liczba utworów (1-based)
    uint8_t  startingSong;     // Utwór startowy (1-based)
    uint16_t loadAddr;         // Adres załadowania danych (0x8000-0xFFFF)
    uint16_t initAddr;         // Adres rutyny INIT
    uint16_t playAddr;         // Adres rutyny PLAY
    char     songName[32];     // Tytuł (null-terminated)
    char     artist[32];       // Artysta (null-terminated)
    char     copyright[32];    // Prawa autorskie (null-terminated)
    uint16_t speedNTSC;        // Szybkość odtwarzania NTSC (1/1000000 s)
    uint8_t  bankValues[8];    // Wartości bankswitch (0 = brak bankswitch)
    uint16_t speedPAL;         // Szybkość odtwarzania PAL (1/1000000 s)
    uint8_t  palNtscBits;      // bit0: PAL, bit1: dual
    uint8_t  extraChipFlags;   // Dodatkowe układy dźwiękowe
    uint8_t  reserved[4];      // Zarezerwowane (0x00)
};
#pragma pack(pop)

static_assert(sizeof(NSFHeader) == 0x80, "NSFHeader musi mieć 128 bajtów");

struct NSFFile {
    NSFHeader header;
    std::vector<uint8_t> data;  // Dane muzyczne (od offsetu 0x80)

    // Pomocniki
    std::string name()      const { return safeStr(header.songName, 32); }
    std::string artist()    const { return safeStr(header.artist,   32); }
    std::string copyright() const { return safeStr(header.copyright,32); }

    bool isBankswitched() const {
        for (int i = 0; i < 8; i++)
            if (header.bankValues[i] != 0) return true;
        return false;
    }

    bool isPAL()     const { return  (header.palNtscBits & 0x01); }
    bool isDualMode()const { return  (header.palNtscBits & 0x02); }

    // Oblicza częstotliwość odtwarzania PLAY w Hz
    double playRateHz(bool pal = false) const {
        uint16_t speed = pal ? header.speedPAL : header.speedNTSC;
        if (speed == 0) speed = pal ? NES::NSF_SPEED_PAL : NES::NSF_SPEED_NTSC;
        return 1000000.0 / speed;
    }

    // Oblicza ile cykli CPU między wywołaniami PLAY
    uint32_t playCyclesPerCall(bool pal = false) const {
        uint32_t cpuClock = pal ? NES::CPU_CLOCK_PAL : NES::CPU_CLOCK_NTSC;
        uint16_t speed    = pal ? header.speedPAL     : header.speedNTSC;
        if (speed == 0) speed = pal ? NES::NSF_SPEED_PAL : NES::NSF_SPEED_NTSC;
        // cykle = cpuClock * speed_us / 1000000
        return (uint32_t)((uint64_t)cpuClock * speed / 1000000);
    }

private:
    static std::string safeStr(const char* buf, int maxLen) {
        int len = 0;
        while (len < maxLen && buf[len] != '\0') len++;
        return std::string(buf, len);
    }
};

class NSFLoader {
public:
    // Wczytuje plik NSF. Zwraca false jeśli plik jest nieprawidłowy.
    static bool load(const std::string& path, NSFFile& out) {
        std::ifstream f(std::filesystem::u8path(path), std::ios::binary);
        if (!f.is_open()) {
            std::cerr << "[NSF] Nie można otworzyć pliku: " << path << "\n";
            return false;
        }

        // Wczytaj nagłówek
        f.read(reinterpret_cast<char*>(&out.header), sizeof(NSFHeader));
        if (f.gcount() < (std::streamsize)sizeof(NSFHeader)) {
            std::cerr << "[NSF] Plik za krótki (niepełny nagłówek)\n";
            return false;
        }

        // Weryfikacja sygnatury
        if (std::strncmp(out.header.magic, "NESM\x1A", 5) != 0) {
            std::cerr << "[NSF] Nieprawidłowa sygnatura. Oczekiwano: 4E 45 53 4D 1A, odczytano: ";
            for (int i = 0; i < 5; i++)
                std::cerr << std::hex << std::uppercase
                          << ((unsigned)(unsigned char)out.header.magic[i]) << " ";
            std::cerr << std::dec << "\n";
            std::cerr << "[NSF] Ścieżka: " << path << "\n";
            return false;
        }

        // Wczytaj dane muzyczne
        out.data.assign(std::istreambuf_iterator<char>(f),
                        std::istreambuf_iterator<char>());


        return true;
    }
};
