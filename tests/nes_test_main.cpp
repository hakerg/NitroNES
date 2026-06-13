#include "NESTestRunner.h"
#include <iostream>
#include <string>
#include <vector>
#include <filesystem>

namespace fs = std::filesystem;

static std::vector<std::string> collectRoms(const std::string& path) {
    std::vector<std::string> roms;
    if (fs::is_regular_file(path)) {
        roms.push_back(path);
    } else if (fs::is_directory(path)) {
        for (const auto& entry : fs::recursive_directory_iterator(path)) {
            if (entry.is_regular_file() && entry.path().extension() == ".nes")
                roms.push_back(entry.path().string());
        }
        std::sort(roms.begin(), roms.end());
    }
    return roms;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Uzycie: nes_tests <rom.nes|katalog> [...]\n";
        return 1;
    }

    for (int i = 1; i < argc; ++i) {
        for (const auto& rom : collectRoms(argv[i])) {
            std::cout << "\n--- " << rom << "\n";

            NESTestRunner runner(rom);
            std::cout << runner.run() << "\n";
        }
    }

    return 0;
}
