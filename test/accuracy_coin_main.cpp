#include "NESHeadlessSystem.h"
#include "AccuracyCoinData.h"
#include <iostream>
#include <string>
#include <cstdint>
#include <cstdlib>
#include <array>

namespace {

constexpr const char* DEFAULT_ROM =
    "C:/Users/PC COMPUTER/source/repos/NES Emulator/"
    "nes-test-roms-master/AccuracyCoin-main/AccuracyCoin.nes";

// Controller button bits (as returned by readController; bit7=A ... bit0=Right).
constexpr uint8_t BTN_START = 0x10;

constexpr int FRAMES_BOOT    = 5  * 60;   // wait 5 seconds
constexpr int FRAMES_RUNNING = 120 * 60;  // wait 2 minutes for all tests

// Table geometry on nametable 0 (see AccuracyCoin.asm result drawing).
constexpr int PAGE_COUNT      = 20;
constexpr int MAX_TESTS       = 10;
constexpr int COL_FIRST       = 6;   // page p -> column 6 + p
constexpr int ROW_FIRST       = 8;   // test t -> row 8 + t
constexpr uint8_t TILE_TLCORNER = 0xD0;
constexpr uint8_t TILE_BLCORNER = 0xD2;
constexpr uint8_t TILE_SKIP     = 0xC9;
constexpr uint8_t TILE_PASS     = 0xFE;

enum class Cell { Empty, Pass, Fail, Skip };

struct Decoded {
    Cell    state[PAGE_COUNT][MAX_TESTS];
    int     failCode[PAGE_COUNT][MAX_TESTS];     // numeric error code for fails
    int     variant[PAGE_COUNT][MAX_TESTS];      // numeric variant code, -1 if none
    bool    tableDrawn = false;
    int     passed = 0, failed = 0, skipped = 0;
};

char codeToChar(int n) { return n < 10 ? char('0' + n) : char('A' + (n - 10)); }

void runFrames(NESHeadlessSystem& nes, int frames) {
    for (int f = 0; f < frames; ++f) nes.tickFrame();
}

Decoded decode(NESHeadlessSystem& nes) {
    Decoded d{};
    std::array<uint8_t, 960> nt{};
    std::array<uint8_t, 256> oam{};
    nes.dumpNametable(nt.data(), 0);
    nes.dumpOAM(oam.data());

    auto tileAt = [&](int row, int col) -> uint8_t { return nt[row * 32 + col]; };

    d.tableDrawn = (tileAt(7, 5) == TILE_TLCORNER) && (tileAt(18, 5) == TILE_BLCORNER);

    for (int p = 0; p < PAGE_COUNT; ++p)
        for (int t = 0; t < MAX_TESTS; ++t) {
            d.variant[p][t]  = -1;
            d.failCode[p][t] = -1;
            uint8_t tile = tileAt(ROW_FIRST + t, COL_FIRST + p);
            if (tile == TILE_PASS)       { d.state[p][t] = Cell::Pass; ++d.passed; }
            else if (tile == TILE_SKIP)  { d.state[p][t] = Cell::Skip; ++d.skipped; }
            else if (tile >= 0x40 && tile <= 0x7F) {
                d.state[p][t] = Cell::Fail;
                d.failCode[p][t] = tile - 0x40;
                ++d.failed;
            } else {
                d.state[p][t] = Cell::Empty;
            }
        }

    // Variant (blue) codes are drawn as sprites overlaying passing cells.
    for (int s = 0; s < 64; ++s) {
        uint8_t y    = oam[s * 4 + 0];
        uint8_t tile = oam[s * 4 + 1];
        uint8_t x    = oam[s * 4 + 3];
        int dy = (int)y - 0x3F;
        int dx = (int)x - 0x30;
        if (dy < 0 || dx < 0 || (dy % 8) || (dx % 8)) continue;
        int t = dy / 8, p = dx / 8;
        if (p >= 0 && p < PAGE_COUNT && t >= 0 && t < MAX_TESTS)
            d.variant[p][t] = tile;
    }
    return d;
}

void report(const Decoded& d) {
    std::cout << "================ AccuracyCoin ================\n";

    if (!d.tableDrawn) {
        std::cout << "HANG / NO RESULTS: the result table was not drawn.\n"
                     "The emulator likely froze before completing all tests "
                     "(no table frame, no \"Tests passed\" line).\n";
        return;
    }

    int total = d.passed + d.failed + d.skipped;
    std::cout << "TESTS PASSED: " << d.passed << " / " << total
              << " (failed: " << d.failed
              << ", skipped: " << d.skipped << ")\n\n";

    for (int p = 0; p < PAGE_COUNT; ++p) {
        const AcPage& page = AC_PAGES[p];
        std::cout << "--- Page " << (p + 1) << ": " << page.name << " ---\n";

        bool isDraw = true;
        for (int t = 0; t < page.testN; ++t)
            if (page.tests[t].errN != 0 || page.tests[t].okN != 0) isDraw = false;
        if (isDraw) {
            std::cout << "  (DRAW page - informational only, no pass/fail results)\n\n";
            continue;
        }

        bool any = false;
        for (int t = 0; t < MAX_TESTS; ++t) {
            const char* testName = (t < page.testN) ? page.tests[t].name : "(unknown test)";

            if (d.state[p][t] == Cell::Fail) {
                any = true;
                int n = d.failCode[p][t];
                char c = codeToChar(n);
                const char* desc = (t < page.testN)
                    ? acLookup(page.tests[t].err, page.tests[t].errN, c) : nullptr;
                std::cout << "  [test " << (t + 1) << "] " << testName
                          << " - FAIL code " << c << ": "
                          << (desc ? desc : "(no description for this code)") << "\n";
            } else if (d.state[p][t] == Cell::Pass && d.variant[p][t] >= 0) {
                any = true;
                int n = d.variant[p][t];
                char c = codeToChar(n);
                const char* desc = (t < page.testN)
                    ? acLookup(page.tests[t].ok, page.tests[t].okN, c) : nullptr;
                std::cout << "  [test " << (t + 1) << "] " << testName
                          << " - PASS (variant " << c << "): "
                          << (desc ? desc : "(acceptable variant behavior)") << "\n";
            } else if (d.state[p][t] == Cell::Skip) {
                any = true;
                std::cout << "  [test " << (t + 1) << "] " << testName
                          << " - SKIPPED\n";
            }
        }
        if (!any) std::cout << "  all tests passed\n";
        std::cout << "\n";
    }
}

} // namespace

int main(int argc, char* argv[]) {
    const std::string rom = (argc >= 2) ? argv[1] : DEFAULT_ROM;
    std::cout << "Loading: " << rom << "\n";

    try {
        NESHeadlessSystem nes(rom);

        runFrames(nes, FRAMES_BOOT);          // boot + menu (5 s)

        nes.setController1(BTN_START);        // press Start (run all tests)
        runFrames(nes, 1);
        nes.setController1(0x00);             // release

        runFrames(nes, FRAMES_RUNNING);       // let all tests run (2 min)

        Decoded d = decode(nes);
        report(d);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    //system("pause");
    return 0;
}



