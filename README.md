# NitroNES

A high-accuracy, low-latency NES/Famicom emulator for Windows. Built in C++23 with a hardware-accurate component-per-class architecture. SDL3 and Dear ImGui are fetched automatically at build time.

## Features

### 🎯 Scanline Sync (Beam Racing)
Real scanline-level synchronization with the host display, matching the emulated PPU's scanline output to the physical monitor's refresh. Eliminates traditional frame-buffer latency — input lag is measured in scanlines, not frames.

### 🌍 NTSC / PAL / Dendy
Full support for all three console regions with cycle-accurate timing derived analytically from master clock frequencies:
- **NTSC** (RP2A03/2C02): 60 Hz, 262 scanlines, odd-frame skip, standard OAM corruption
- **PAL** (RP2A07/2C07): 50 Hz, 312 scanlines, 3.2 PPU dots per CPU cycle, OAM refresh during vblank, no OAM corruption, swapped emphasis bits, black borders
- **Dendy** (UA6527P/UA6538): ~50 Hz, 312 scanlines, NMI at scanline 291, NTSC-timed APU, PAL-style borders/emphasis, NTSC-style OAM corruption

Switch regions at runtime: Emulation → System, or `system:pal` in `nes_test`.

### 💾 Quick Save / Quick Load
10 save-state slots per ROM, stored as `saves/<rom>.0.sav` through `.9.sav`. Serializes the entire emulator state (CPU, PPU, APU, mapper, RAM). No per-mapper code required.

### ⏪ Rewind
Hold Backspace to rewind gameplay in real time. Keeps up to 10,000 frames of delta-compressed history. Works with both NES and NSF files.

### ⏩ Speed Control
Hold Tab to activate the speed configured in sync settings. Hold Shift+Tab to slow down. Backspace+Tab for fast rewind.

### 📂 Drag & Drop
Drag a `.nes` or `.nsf` file onto the window to load it instantly.

### 🧪 Test Suite
NitroNES passes the full NES test ROM corpus. Run `python run_tests.py` for the current state.

- All blargg CPU tests (5 suites)
- All blargg APU tests (11 suites)
- All blargg PPU tests (vbl_nmi, ppu_tests, sprite_hit, sprite_overflow)
- All PPU timing tests (open bus, vbl_nmi_timing, read_buffer, oam_read, DMA)
- All PAL APU tests (10 ROMs, cycle-accurate)
- CPU reset / APU reset tests
- AccuracyCoin: 141/141 cycle-exact PPU sub-tests
- Holy Mapperel mapper tests (35 ROMs, 16 mappers)

Two MMC3 rev B (Sharp) tests are excluded — the emulator targets rev A/MMC6; the revisions are mutually exclusive at the hardware level.

### 🎵 NSF Player
Full NSF (Nintendo Sound Format) support with song navigation (Left/Right), pause (Space), speed control, and rewind. PAL/Dendy clock rates supported for PAL-only NSFs. VRC6 and VRC7 expansion audio included.

### 🔊 Expansion Audio
- **VRC6** (Castlevania III JP, Madara, Esper Dream 2) — 2 pulse + sawtooth channels
- **VRC7 / YM2413** (Lagrange Point) — 6-channel FM synthesis
- **MMC5** (Castlevania III US, Just Breed) — 2 pulse channels
- **Namco 163 / N163** (Rolling Thunder, Erika to Satoru) — up to 8 wavetable channels
- **Sunsoft 5B** (Gimmick!) — 3 square-wave channels

### 🗺️ Mapper Coverage
96 mappers implemented, covering virtually the entire licensed NES library and most unlicensed carts:
- **MMC1** (SxROM/SUROM): Zelda, Metroid, Final Fantasy — with SUROM 512KB PRG
- **MMC3** (TxROM): Super Mario Bros 3, Mega Man 3-6, Kirby's Adventure — rev A with cycle-accurate IRQ
- **MMC5** (ExROM): Castlevania III US, Just Breed — with expansion audio
- **Konami VRC** (VRC2/4/6/7): Castlevania III JP, Lagrange Point, Gradius II
- **Sunsoft** (FME-7, Sunsoft-5B): Batman: Return of the Joker, Gimmick!
- **Namco** (N163, 108, 109, 118, 119): Full Namco library
- **Bandai** (FCG, LZ93D50, Oeka Kids): Datach/Jump, Dragon Ball Z, Oeka Kids tablet

### 🖥️ GUI
Dear ImGui interface with:
- File menu: open, reload, close, save/load state slots, quit
- Emulation menu: pause, reset, system region (NTSC/PAL/Dendy), tools (file info, memory viewer)
- Settings: language (Polish/English), sync, graphics, audio, controls
- Windowed or fullscreen

### 🛠️ CLI Tools
- **`nes_test`**: Scriptable test harness — frame stepping, ASCII screen dumps, pixel captures, memory inspection, CPU/PPU/DMA/APU traces. Supports `.nes`, `.nsf`, `.asm` (auto-build), `.s` (auto-build).
- **`accuracy_coin`**: Runs the full AccuracyCoin test suite and prints a pass/fail report.

## Building

### Prerequisites
- CMake 3.24+, Ninja, Git
- SDL3 and Dear ImGui are fetched automatically

### Build
```bash
cmake -S . -B build/release -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja -C build/release
```

Produces:
- `build/release/NitroNES.exe` — main emulator GUI
- `build/release/nes_test.exe` — CLI test harness
- `build/release/accuracy_coin.exe` — AccuracyCoin batch runner

No runtime DLLs required — the executable is statically linked.

## Testing
```bash
# Full test suite
python run_tests.py

# Just AccuracyCoin
./build/release/accuracy_coin.exe

# Individual test with trace
./build/release/nes_test.exe mytest.nes trace:cpu:on frames:600
```

## License
The emulator code (in `src/` and `test/`) is free for anyone to use.

Third-party dependencies, fetched at build time, carry their own licenses:
- [SDL3](https://github.com/libsdl-org/SDL) — zlib license
- [Dear ImGui](https://github.com/ocornut/imgui) — MIT license

The `MesenCE/` and `metalnes/` directories are unrelated third-party emulator source trees kept locally for reference only; they are not part of this project and remain under their original authors' licenses.
