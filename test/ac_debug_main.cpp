// ac_debug — runs a single AccuracyCoin test via menu navigation and dumps CPU RAM.
//
// Usage: ac_debug [downs] [dumpStart] [dumpLen]
//   downs     - number of D-Pad Down presses from the page-13 header to the test row
//               (Explicit DMA Abort = 9, Implicit DMA Abort = 10)
//   dumpStart - first zero-page address to dump (default 0x50)
//   dumpLen   - number of bytes to dump (default 16)
#include "NESHeadlessSystem.h"
#include <cstdio>
#include <cstdlib>
#include <string>

namespace {
constexpr const char* DEFAULT_ROM =
    "C:/Users/PC COMPUTER/source/repos/NES Emulator/"
    "nes-test-roms-master/AccuracyCoin-main/AccuracyCoin.nes";

constexpr uint8_t BTN_A      = 0x80;
constexpr uint8_t BTN_RIGHT  = 0x01;
constexpr uint8_t BTN_DOWN   = 0x04;

void run(NESHeadlessSystem& nes, int frames) {
    double dt;
    for (int i = 0; i < frames; ++i) nes.tickFrame(dt);
}

void press(NESHeadlessSystem& nes, uint8_t btn) {
    nes.setController1(btn);
    run(nes, 3);
    nes.setController1(0x00);
    run(nes, 3);
}
} // namespace

int main(int argc, char* argv[]) {
    int rights = (argc > 1) ? std::atoi(argv[1]) : 12;
    int downs = (argc > 2) ? std::atoi(argv[2]) : 9;
    int dumpStart = (argc > 3) ? (int)std::strtol(argv[3], nullptr, 0) : 0x50;
    int dumpLen = (argc > 4) ? std::atoi(argv[4]) : 16;

    NESHeadlessSystem nes(DEFAULT_ROM);
    run(nes, 200);                       // boot

    // From page 1 header, move right to the target page.
    for (int i = 0; i < rights; ++i) press(nes, BTN_RIGHT);
    // Move the cursor down to the target test row.
    for (int i = 0; i < downs; ++i) press(nes, BTN_DOWN);
    // Run the highlighted test.
    press(nes, BTN_A);
    run(nes, 600);                       // let the test run

    std::printf("PC=%04X  RAM[%02X..]:", nes.cpuPC(), dumpStart);
    for (int i = 0; i < dumpLen; ++i)
        std::printf(" %02X", nes.peekRAM((uint16_t)(dumpStart + i)));
    std::printf("\n");
    return 0;
}


