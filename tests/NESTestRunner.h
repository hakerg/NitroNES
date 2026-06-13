#pragma once
#include "NESHeadlessSystem.h"
#include <string>
#include <algorithm>

class NESTestRunner {
public:
    explicit NESTestRunner(std::string romPath)
        : romPath(std::move(romPath)) {}

    std::string run(int frames = 30 * 60) {
        NESHeadlessSystem nes;

        if (!nes.loadFile(romPath))
            return "Nie udalo sie zaladowac: " + romPath;

        for (int f = 0; f < frames; ++f) {
            double dt;
            nes.tickFrame(dt);
        }

        return readScreen(nes);
    }

private:
    std::string romPath;

    static std::string readScreen(NESHeadlessSystem& nes) {
        uint8_t tiles[960]{};
        nes.dumpNametable(tiles, 0);

        std::string raw;
        for (int row = 0; row < 30; ++row) {
            std::string line;
            for (int col = 0; col < 32; ++col) {
                uint8_t t = tiles[row * 32 + col];
                line += (t >= 0x20 && t <= 0x7E) ? static_cast<char>(t) : ' ';
            }
            auto last = line.find_last_not_of(' ');
            if (last != std::string::npos)
                raw += line.substr(0, last + 1) + '\n';
        }

        auto start = raw.find_first_not_of('\n');
        auto end   = raw.find_last_not_of('\n');
        if (start == std::string::npos) return {};
        return raw.substr(start, end - start + 1);
    }
};
