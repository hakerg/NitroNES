// asm_run: compile a .asm file with nesasm3, run it in the headless NES emulator,
// print the screen contents to stdout, and optionally dump a cycle trace.
//
// Usage:
//   asm_run [options] <file.asm>
//
// Options:
//   --nesasm PATH         path to nesasm.exe (default: auto-detect)
//   --frames  N           number of frames to emulate (default: 300)
//   --trigger SPEC        enable cycle tracer with trigger:
//                           oam          - OAM DMA start (write to $4014)
//                           dmc          - DMC DMA start
//                           nmi          - NMI edge (line goes low)
//                           pc:XXXX      - CPU PC reaches hex address XXXX
//                           addr:XXXX    - any write to hex address XXXX
//   --pre     N           pre-trigger ring buffer cycles to dump (default: 300)
//   --post    N           post-trigger cycles to dump (default: 300)
//   --trace-file FILE     output file for trace (default: trace.tsv)

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <stdexcept>
#include "../test/TracedNESHeadlessSystem.h"

namespace fs = std::filesystem;

static fs::path findNesasm(const fs::path& binaryDir) {
    for (const auto& candidate : {
        binaryDir / "nesasm.exe",
        binaryDir / "../nes-test-roms-master/AccuracyCoin-main/nesasm.exe",
        binaryDir / "../../nes-test-roms-master/AccuracyCoin-main/nesasm.exe",
        fs::path("nesasm.exe"),
    }) {
        std::error_code ec;
        if (fs::exists(candidate, ec))
            return fs::absolute(candidate);
    }
    throw std::runtime_error("nesasm.exe not found. Use --nesasm PATH to specify.");
}

static CycleTracer::Config parseTrigger(const std::string& spec,
                                        int pre, int post,
                                        const std::string& outFile) {
    CycleTracer::Config cfg;
    cfg.preCycles  = pre;
    cfg.postCycles = post;
    cfg.outFile    = outFile;

    if (spec == "oam") {
        cfg.triggerKind = CycleTracer::TriggerKind::OAMDMAStart;
    } else if (spec == "dmc") {
        cfg.triggerKind = CycleTracer::TriggerKind::DMCDMAStart;
    } else if (spec == "nmi") {
        cfg.triggerKind = CycleTracer::TriggerKind::NMIEdge;
    } else if (spec.substr(0, 3) == "pc:") {
        cfg.triggerKind = CycleTracer::TriggerKind::PCValue;
        cfg.triggerAddr = static_cast<uint16_t>(std::stoul(spec.substr(3), nullptr, 16));
    } else if (spec.substr(0, 5) == "addr:") {
        cfg.triggerKind = CycleTracer::TriggerKind::AddrWrite;
        cfg.triggerAddr = static_cast<uint16_t>(std::stoul(spec.substr(5), nullptr, 16));
    } else {
        throw std::runtime_error("Unknown trigger spec: " + spec);
    }
    return cfg;
}

static std::string readScreen(TracedNESHeadlessSystem& nes) {
    uint8_t tiles[960]{};
    nes.dumpNametable(tiles, 0);

    std::string result;
    for (int row = 0; row < 30; ++row) {
        std::string line;
        for (int col = 0; col < 32; ++col) {
            uint8_t t = tiles[row * 32 + col];
            line += (t >= 0x20 && t <= 0x7E) ? static_cast<char>(t) : ' ';
        }
        auto last = line.find_last_not_of(' ');
        if (last != std::string::npos)
            result += line.substr(0, last + 1) + '\n';
    }
    auto start = result.find_first_not_of('\n');
    auto end   = result.find_last_not_of('\n');
    if (start == std::string::npos) return "(empty screen)";
    return result.substr(start, end - start + 1);
}

int main(int argc, char* argv[]) {
    std::string asmFile;
    std::string nesasmOverride;
    std::string triggerSpec;
    std::string traceFile = "trace.tsv";
    int frames    = 300;
    int preCycles = 300;
    int postCycles= 300;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--nesasm"    && i+1 < argc) { nesasmOverride = argv[++i]; }
        else if (arg == "--frames" && i+1 < argc) { frames = std::stoi(argv[++i]); }
        else if (arg == "--trigger"&& i+1 < argc) { triggerSpec = argv[++i]; }
        else if (arg == "--pre"    && i+1 < argc) { preCycles = std::stoi(argv[++i]); }
        else if (arg == "--post"   && i+1 < argc) { postCycles = std::stoi(argv[++i]); }
        else if (arg == "--trace-file" && i+1 < argc) { traceFile = argv[++i]; }
        else if (arg[0] != '-') { asmFile = arg; }
        else { fprintf(stderr, "Unknown option: %s\n", arg.c_str()); return 1; }
    }

    if (asmFile.empty()) {
        fprintf(stderr, "Usage: asm_run [options] <file.asm>\n");
        return 1;
    }

    fs::path asmPath   = fs::absolute(asmFile);
    fs::path asmDir    = asmPath.parent_path();
    fs::path nesPath   = asmPath;
    nesPath.replace_extension(".nes");

    fs::path binaryDir = fs::path(argv[0]).parent_path();
    fs::path nesasm    = nesasmOverride.empty()
                         ? findNesasm(binaryDir)
                         : fs::absolute(nesasmOverride);

    // compile — on Windows, cmd.exe requires the whole command in outer quotes
    // when any argument contains spaces: "\"exe\" \"arg\""
    std::string cmd = "\"\"" + nesasm.string() + "\" \"" + asmPath.string() + "\"\"";
    fprintf(stderr, "[asm_run] compiling: %s\n", cmd.c_str());

    std::error_code ec;
    fs::current_path(asmDir, ec);
    int ret = std::system(cmd.c_str());
    if (ret != 0) {
        fprintf(stderr, "[asm_run] nesasm failed (exit %d)\n", ret);
        return 1;
    }

    if (!fs::exists(nesPath, ec)) {
        fprintf(stderr, "[asm_run] .nes not found at: %s\n", nesPath.string().c_str());
        return 1;
    }

    TracedNESHeadlessSystem nes(nesPath.string());

    if (!triggerSpec.empty()) {
        nes.attachTracer(parseTrigger(triggerSpec, preCycles, postCycles, traceFile));
    }

    for (int f = 0; f < frames; ++f) {
        double dt;
        nes.tickFrame(dt);
        if (!triggerSpec.empty() && nes.traceIsDone()) {
            fprintf(stderr, "[asm_run] trace trigger fired at frame %d, stopping.\n", f);
            break;
        }
    }

    if (!triggerSpec.empty() && !nes.traceIsDone())
        fprintf(stderr, "[asm_run] warning: trigger never fired. No trace written.\n");
    else if (!triggerSpec.empty())
        fprintf(stderr, "[asm_run] trace written to: %s\n", traceFile.c_str());

    puts(readScreen(nes).c_str());
    return 0;
}


