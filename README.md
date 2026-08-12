# NitroNES

A high-accuracy, low-latency NES/Famicom emulator for Windows, built with a hardware-accurate component-per-class architecture. Written in modern C++23 with zero external runtime dependencies beyond SDL3 and Dear ImGui (both fetched at build time).

## Features

### 🎯 Scanline Sync (Beam Racing)
NitroNES implements real scanline-level synchronization with the host display, matching the emulated PPU's scanline output to the physical monitor's refresh. This eliminates the traditional frame-buffer latency — input lag is measured in scanlines, not frames. When fullscreen on a variable-refresh-rate display, the emulator synchronizes every scanline for near-zero perceived latency.

### 🌍 NTSC / PAL / Dendy
Full support for all three console regions with cycle-accurate timing derived analytically from master clock frequencies:
- **NTSC** (RP2A03/2C02): 60 Hz, 262 scanlines, odd-frame skip, standard OAM corruption
- **PAL** (RP2A07/2C07): 50 Hz, 312 scanlines, 3.2 PPU dots per CPU cycle, OAM refresh during vblank, no OAM corruption, swapped emphasis bits, black borders
- **Dendy** (UA6527P/UA6538): ~50 Hz, 312 scanlines, NMI at scanline 291, NTSC-timed APU, PAL-style borders/emphasis, NTSC-style OAM corruption

Switch regions at runtime via the GUI (Emulation → System) or `nes_test` CLI (`system:pal`).

### 💾 Quick Save / Quick Load
10 save-state slots per ROM, stored as `saves/<rom>.0.sav` through `.9.sav`. Serializes the entire emulator state (CPU, PPU, APU, mapper, RAM) — no per-mapper code required. Save states survive across emulator restarts.

### ⏪ Rewind
Hold Backspace to rewind gameplay in real time. Keeps up to 10,000 frames of delta-compressed history. Works with both NES and NSF files. Audio plays backwards during rewind.

### ⏩ Speed Control
- **Tab**: Speed up (2×, 4×, 8×, uncapped)
- **Shift+Tab**: Slow down
- **Backspace+Tab**: Fast rewind
- **Backspace+Shift+Tab**: Slow rewind

### 🧪 Test Suite Coverage
NitroNES passes the vast majority of the NES test corpus. The automated test runner (`run_tests.py`) executes 142 ROMs with unambiguous text results:

- ✅ All 5 blargg CPU suites (instr_test, instr_timing, cpu_timing, branch_timing, CPU interrupts)
- ✅ All 11 blargg APU suites (length counter, envelope, sweep, DMC, noise, timing)
- ✅ All blargg PPU suites (vbl_nmi, ppu_tests, sprite_hit, sprite_overflow)
- ✅ All PPU timing tests (open bus, vbl_nmi_timing, read_buffer, oam_read, DMA tests)
- ✅ All 10 PAL APU tests (blargg, cycle-accurate timing with PAL frame sequencer)
- ✅ AccuracyCoin: 140/141 cycle-exact PPU sub-tests
- ✅ CPU reset / APU reset tests
- ⚠️ 2 MMC3 rev B tests (mutually exclusive — emulating rev A/MMC6)
- ⚠️ Holy Mapperel mapper tests (partial — core banking/PRG-RAM/IRQ checks in progress)

Current status: **105 PASS / 34 known issues** (2 rev-B exclusives + 32 mapper edge cases under active development).

### 🎵 NSF Player
Full NSF (Nintendo Sound Format) support with:
- Automatic song detection and playback
- Previous/next song navigation (Left/Right arrows)
- Pause (Space)
- VRC6 and VRC7 expansion audio
- Speed control and rewind
- PAL/Dendy clock rate support for PAL-only NSFs

### 🔊 Expansion Audio
- **VRC6** (Castlevania III JP, Madara, Esper Dream 2) — 2 pulse + sawtooth channels
- **VRC7 / YM2413** (Lagrange Point) — 6-channel FM synthesis
- **MMC5** (Castlevania III US, Just Breed) — 2 pulse channels
- **Namco 163 / N163** (Rolling Thunder, Erika to Satoru) — up to 8 wavetable channels
- **Sunsoft 5B** (Gimmick!) — 3 square-wave channels

### 🗺️ Mapper Coverage
96 mappers implemented, covering virtually the entire licensed NES library and most unlicensed carts. Notable mappers:
- **MMC1** (SxROM/SUROM): Zelda, Metroid, Final Fantasy — with SUROM 512KB PRG
- **MMC3** (TxROM): Super Mario Bros 3, Mega Man 3-6, Kirby's Adventure — rev A with cycle-accurate IRQ
- **MMC5** (ExROM): Castlevania III US, Just Breed — with expansion audio
- **Konami VRC series** (VRC2/4/6/7): Castlevania III JP, Lagrange Point, Gradius II
- **Sunsoft** (FME-7, Sunsoft-5B): Batman: Return of the Joker, Gimmick!
- **Namco** (N163, 108, 109, 118, 119): Full Namco library
- **Bandai** (FCG, LZ93D50, Oeka Kids): Datach/Jump, Dragon Ball Z, Oeka Kids tablet

### 🛠️ Build & Hacking Tools
- **`nes_test`**: Scriptable test harness with interactive stdin mode. Supports frame stepping, ASCII screen dumps, pixel captures, memory inspection, symbol-annotated CPU/PPU/DMA/APU tracing, and automated AccuracyCoin menu navigation. Accepts `.nes`, `.nsf`, `.asm` (auto-build via nesasm3), and `.s` (auto-build via ca65/ld65).
- **`accuracy_coin`**: Runs the full AccuracyCoin test suite and prints a pass/fail report with per-test detail.
- **`run_tests.py`**: Builds the project and runs all 142 automated test ROMs, printing a current-state report.

### 🖥️ GUI
Dear ImGui-powered interface with:
- Drag-and-drop ROM loading
- File browser with recent files
- Emulation controls (pause, reset, speed, system region)
- NSF song browser with metadata display
- Input configuration (keyboard + controllers)
- Video settings (scanline sync, aspect ratio)
- Audio settings (sample rate, buffer size)
- Bilingual UI: Polish / English

### 💻 CLI Tools
Two headless CLI tools for automation and testing:
- **`nes_test`**: Full control via stdin — frame stepping, memory dumps, pixel captures, CPU/PPU/DMA/APU traces
- **`accuracy_coin`**: One-shot AccuracyCoin test suite runner

Both support PAL/Dendy switching via `system:` command and NSF playback via `NSFPlayer` core.

## Building

### Prerequisites
- CMake 3.24+
- Ninja (preferred) or Visual Studio 2022+
- Git (SDL3 and Dear ImGui are fetched automatically via CMake FetchContent)

### Build (Ninja)
```bash
cmake -S . -B build/release -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja -C build/release
```

This produces:
- `build/release/NitroNES.exe` — main emulator GUI
- `build/release/nes_test.exe` — CLI test harness
- `build/release/accuracy_coin.exe` — AccuracyCoin batch runner

### Build (Visual Studio)
```bash
cmake -S . -B build/vs -G "Visual Studio 17 2022"
cmake --build build/vs --config Release
```

### Dependencies
All dependencies are fetched at build time via CMake FetchContent:
- [SDL3](https://github.com/libsdl-org/SDL) (main branch, statically linked)
- [Dear ImGui](https://github.com/ocornut/imgui) (docking branch)

No runtime DLLs required — the executable is fully self-contained (~3 MB).

## Codebase
- **Language**: C++23
- **Architecture**: Header-only core, one class per hardware component (CPU6502, PPU2C02, APU, DMA, etc.)
- **Size**: ~23,000 lines of C++, 142 source files
- **Mappers**: 96 mapper headers in `src/core/mappers/`
- **Style**: Minimal state, no comments, hardware-faithful component mirroring

## Testing
```bash
# Full test suite (~2 minutes)
python run_tests.py

# Just AccuracyCoin
./build/release/accuracy_coin.exe

# Individual test with trace
./build/release/nes_test.exe mytest.nes trace:cpu:on frames:600

# AccuracyCoin single test with navigation
./build/release/nes_test.exe AccuracyCoin.asm ac:7:3 frames:600 trace:cpu:on
```

## License
Proprietary. Source available for reference.

---

*NitroNES — accuracy without compromise, latency without excuses.*
