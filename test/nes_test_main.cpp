#include "NESTestRunner.h"
#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <algorithm>
#include <cctype>

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

static bool needsReset(const std::string& output) {
    std::string lower(output.size(), '\0');
    std::ranges::transform(output, lower.begin(),
                           [](unsigned char c){ return (char)std::tolower(c); });
    return lower.find("press reset") != std::string::npos;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Uzycie: nes_tests <rom.nes|katalog> [...]\n";
        return 1;
    }

    for (int i = 1; i < argc; ++i) {
        for (const auto& rom : collectRoms(argv[i])) {
            std::cout << "\n--- " << rom << "\n";
            try {
                NESTestRunner runner(rom);
                std::string result = runner.run();
                std::cout << result << "\n";

                int resetCount = 0;
                while (needsReset(result)) {
                    ++resetCount;
                    std::cout << "[reset #" << resetCount << "]\n";
                    runner.reset();
                    result = runner.run();
                    std::cout << result << "\n";
                }
            } catch (const std::exception& e) {
                std::cerr << e.what() << "\n";
            }
        }
    }

    //system("pause");
    return 0;
}
