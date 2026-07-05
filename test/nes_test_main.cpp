// nes_test — script-driven NES test harness.
//
// Usage:
//   nes_test <rom-or-asm-or-nsf> [command]...
//
// Input file handling:
//   *.nes            run as-is
//   *.nsf            run through the NSF player core (no PPU/screen)
//   *.asm            assemble with nesasm3 (source dir contents copied to a
//                    temp build dir; original tree is never modified)
//   *.s              assemble + link with ca65/ld65 (artifacts go to a temp
//                    build dir so the source tree is never modified)
//
// A symbol file next to the resulting .nes (`.fns` for nesasm or `.lbl` for
// ld65 VICE format) is loaded automatically; symbol names then appear in the
// CPU trace.
//
// Commands (run sequentially; one per argv):
//   frames:N            tick N PPU frames (or N play() calls for .nsf)
//   cycles:N            tick N CPU cycles
//   reset               trigger hardware reset
//   screen[:ascii]      dump nametable 0 as hex tile indices (.nes only);
//                       add :ascii to render printable tiles as characters
//   pixels:X:Y:W:H      dump a framebuffer region as RGB hex, one row per line
//                       (.nes only, e.g. FF0000 00FF00 0000FF)
//   mem:ADDR:LEN        dump LEN bytes from CPU bus ADDR (hex/dec/$hex/0xhex)
//   pad1=BTNS           set controller 1 state (BTNS = comma list, empty=clear)
//   pad1+BTN[,BTN..]    press buttons on controller 1
//   pad1-BTN[,BTN..]    release buttons on controller 1
//   pad2=... +... -...  same for controller 2
//   trace:CHAN:STATE    enable/disable trace channel (CHAN=cpu|ppu|dma, STATE=on|off)
//   trace-file:PATH     redirect trace output (default: trace.log)
//   song:N              (.nsf only) initSong(N)
//   next / prev         (.nsf only) switch to next/previous song
//   songinfo            (.nsf only) print song name/artist/copyright
//
// Button names: A, B, SELECT, START, UP, DOWN, LEFT, RIGHT

#include "TestRunner.h"
#include "NESHeadlessNSF.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static fs::path findFirst(const std::vector<fs::path>& candidates) {
    std::error_code ec;
    for (const auto& c : candidates)
        if (fs::exists(c, ec)) return fs::absolute(c);
    return {};
}

static fs::path findNesasm(const fs::path& binaryDir) {
    fs::path p = findFirst({
        binaryDir / "nesasm.exe",
        binaryDir / "../nes-test-roms-master/AccuracyCoin-main/nesasm.exe",
        binaryDir / "../../nes-test-roms-master/AccuracyCoin-main/nesasm.exe",
        fs::path("nesasm.exe"),
    });
    if (p.empty()) throw std::runtime_error("nesasm.exe not found");
    return p;
}

static fs::path findCc65Bin(const fs::path& binaryDir, const std::string& tool) {
    fs::path p = findFirst({
        binaryDir / "../../cc65-snapshot-win32/bin" / (tool + ".exe"),
        binaryDir / "../cc65-snapshot-win32/bin"    / (tool + ".exe"),
        binaryDir / "cc65-snapshot-win32/bin"       / (tool + ".exe"),
        fs::path(tool + ".exe"),
    });
    if (p.empty()) throw std::runtime_error("cc65 tool not found: " + tool);
    return p;
}

static fs::path findUp(const fs::path& start, const std::string& name, int maxDepth = 4) {
    fs::path cur = start;
    std::error_code ec;
    for (int i = 0; i < maxDepth; ++i) {
        fs::path candidate = cur / name;
        if (fs::exists(candidate, ec)) return fs::absolute(candidate);
        if (!cur.has_parent_path() || cur.parent_path() == cur) break;
        cur = cur.parent_path();
    }
    return {};
}

static int runCmd(const std::string& cmd, const fs::path& workDir = {}) {
    std::error_code ec;
    fs::path prev = fs::current_path(ec);
    if (!workDir.empty()) fs::current_path(workDir, ec);
    std::cerr << "[nes_test] $ " << cmd << "\n";
    int ret = std::system(cmd.c_str());
    if (!workDir.empty()) fs::current_path(prev, ec);
    return ret;
}

static fs::path makeBuildDir(const fs::path& srcAbs) {
    fs::path buildDir = fs::temp_directory_path() / "nes_test_build"
                       / std::to_string(std::hash<std::string>{}(srcAbs.string()))
                       / srcAbs.stem();
    std::error_code ec;
    fs::remove_all(buildDir, ec);
    fs::create_directories(buildDir, ec);
    return buildDir;
}

static fs::path buildAsmNesasm(const fs::path& asmPath, const fs::path& binaryDir) {
    fs::path nesasm = findNesasm(binaryDir);
    fs::path asmAbs = fs::absolute(asmPath);
    fs::path buildDir = makeBuildDir(asmAbs);

    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(asmAbs.parent_path(), ec)) {
        if (!entry.is_regular_file()) continue;
        fs::copy_file(entry.path(), buildDir / entry.path().filename(),
                      fs::copy_options::overwrite_existing, ec);
    }

    fs::path asmInBuild = buildDir / asmAbs.filename();
    fs::path nesOut = asmInBuild; nesOut.replace_extension(".nes");

    std::string cmd = "\"\"" + nesasm.string() + "\" \"" + asmInBuild.string() + "\"\"";
    if (runCmd(cmd, buildDir) != 0)
        throw std::runtime_error("nesasm failed");
    if (!fs::exists(nesOut, ec)) throw std::runtime_error("compiled .nes not found");
    return nesOut;
}

static fs::path buildSrcCa65(const fs::path& srcPath, const fs::path& binaryDir) {
    fs::path srcAbs    = fs::absolute(srcPath);
    fs::path srcDir    = srcAbs.parent_path();
    fs::path commonDir = findUp(srcDir, "common");
    fs::path nesCfg    = findUp(srcDir, "nes.cfg");
    if (nesCfg.empty())
        throw std::runtime_error("nes.cfg not found near " + srcAbs.string());

    fs::path buildDir = makeBuildDir(srcAbs);

    fs::path objOut = buildDir / (srcAbs.stem().string() + ".o");
    fs::path nesOut = buildDir / (srcAbs.stem().string() + ".nes");
    fs::path lblOut = buildDir / (srcAbs.stem().string() + ".lbl");

    fs::path ca65 = findCc65Bin(binaryDir, "ca65");
    fs::path ld65 = findCc65Bin(binaryDir, "ld65");

    auto q = [](const fs::path& p){ return "\"" + p.string() + "\""; };

    std::string asmCmd = "\"" + q(ca65) + " -g --feature force_range";
    if (!commonDir.empty()) asmCmd += " -I " + q(commonDir);
    asmCmd += " " + q(srcAbs) + " -o " + q(objOut) + "\"";
    if (runCmd(asmCmd, srcDir) != 0) throw std::runtime_error("ca65 failed");

    std::string lnkCmd = "\"" + q(ld65) + " -C " + q(nesCfg)
                       + " " + q(objOut) + " -o " + q(nesOut)
                       + " -Ln " + q(lblOut) + "\"";
    if (runCmd(lnkCmd, srcDir) != 0) throw std::runtime_error("ld65 failed");

    std::error_code ec;
    if (!fs::exists(nesOut, ec)) throw std::runtime_error(".nes not produced");
    return nesOut;
}

template <typename CoreT>
static int runTest(const fs::path& romPath, const std::vector<std::string>& commands) {
    TestRunner<CoreT> runner(romPath.string());

    std::error_code ec;
    for (const char* symExt : { ".fns", ".lbl" }) {
        fs::path sym = romPath; sym.replace_extension(symExt);
        if (fs::exists(sym, ec)) {
            runner.loadSymbols(sym.string());
            std::cerr << "[nes_test] symbols: " << sym.string() << "\n";
        }
    }

    return runner.run(commands) ? 0 : 1;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr <<
            "Usage: nes_test <rom-or-asm-or-nsf> [command]...\n"
            "Input: .nes (run), .nsf (run NSF player), .asm (nesasm3), .s (ca65+ld65; tmp build dir)\n"
            "Commands: frames:N cycles:N reset screen[:ascii] pixels:X:Y:W:H mem:ADDR:LEN\n"
            "          pad1=BTNS pad1+BTN pad1-BTN (same for pad2)\n"
            "          trace:cpu|ppu|dma:on|off  trace-file:PATH\n"
            "NSF only: song:N next prev songinfo\n"
            "Buttons:  A B SELECT START UP DOWN LEFT RIGHT\n";
        return 1;
    }

    try {
        fs::path input = argv[1];
        std::string ext = input.extension().string();
        for (auto& c : ext) c = (char)std::tolower((unsigned char)c);

        fs::path binaryDir = fs::path(argv[0]).parent_path();
        fs::path romPath = input;
        if      (ext == ".asm") romPath = buildAsmNesasm(input, binaryDir);
        else if (ext == ".s")   romPath = buildSrcCa65(input, binaryDir);

        std::vector<std::string> commands;
        for (int i = 2; i < argc; ++i) commands.emplace_back(argv[i]);

        if (ext == ".nsf") return runTest<NESHeadlessNSF>(romPath, commands);
        return runTest<NESHeadlessSystem>(romPath, commands);
    } catch (const std::exception& e) {
        std::cerr << "[nes_test] " << e.what() << "\n";
        return 1;
    }
}

