#pragma once
#include "KeyChord.h"
#include "core/AudioStream.h"
#include "sdl/ControllerSettings.h"
#include <SDL3/SDL.h>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

struct AppKeyBindings {
    KeyChord pause{SDL_SCANCODE_P};
    KeyChord fullScreen{SDL_SCANCODE_F11};
    KeyChord speedUp{SDL_SCANCODE_TAB};
    KeyChord speedDown{SDL_SCANCODE_TAB, KeyChord::MOD_SHIFT};
    KeyChord reset{};
    KeyChord nsfTogglePause{SDL_SCANCODE_SPACE};
    KeyChord nsfNextSong{SDL_SCANCODE_RIGHT};
    KeyChord nsfPrevSong{SDL_SCANCODE_LEFT};
};

struct AppSettings {
    bool allowScanlineSync = false;
    bool vsync = false;
    bool matchRefreshRate = true;
    int scanlineBufferMs = 10;
    int languageIndex = 0;
    AudioSettings audioSettings;
    ControllerSettings controllers[2] = {ControllerSettings::player1(),
                                         ControllerSettings::player2()};
    AppKeyBindings keys;

    AppSettings() { load(filePath()); }
    ~AppSettings() { save(filePath()); }

    AppSettings(const AppSettings &) = delete;
    AppSettings &operator=(const AppSettings &) = delete;

private:
    static std::filesystem::path filePath() {
        const char *base = std::getenv("LOCALAPPDATA");
        std::filesystem::path dir = base ? std::filesystem::path(base)
                                         : std::filesystem::current_path();
        dir /= "NES Emulator";
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        return dir / "settings.cfg";
    }

    void load(const std::filesystem::path &path) {
        std::ifstream in(path);
        if (!in)
            return;
        std::unordered_map<std::string, std::string> kv;
        std::string line;
        while (std::getline(in, line)) {
            auto eq = line.find('=');
            if (eq == std::string::npos)
                continue;
            kv.emplace(line.substr(0, eq), line.substr(eq + 1));
        }
        applyMap(kv);
    }

    void save(const std::filesystem::path &path) const {
        std::ofstream out(path, std::ios::trunc);
        if (!out)
            return;
        for (const auto &[k, v] : buildMap())
            out << k << '=' << v << '\n';
    }

    template <typename T> static T parse(const std::string &s, T fallback) {
        std::istringstream is(s);
        T v{};
        if (is >> v)
            return v;
        return fallback;
    }

    static SDL_Scancode parseScancode(const std::string &s) {
        return (SDL_Scancode)parse<int>(s, SDL_SCANCODE_UNKNOWN);
    }

    static std::string scStr(SDL_Scancode sc) {
        return std::to_string((int)sc);
    }

    static std::string chordStr(const KeyChord &c) {
        return std::to_string((int)c.scancode) + ":" + std::to_string(c.mods);
    }

    static KeyChord parseChord(const std::string &s) {
        auto colon = s.find(':');
        KeyChord c;
        if (colon == std::string::npos) {
            c.scancode = parseScancode(s);
        } else {
            c.scancode = parseScancode(s.substr(0, colon));
            c.mods = (uint8_t)parse<int>(s.substr(colon + 1), 0);
        }
        return c;
    }

    static void writeController(
        std::vector<std::pair<std::string, std::string>> &out,
        const std::string &prefix, const ControllerSettings &c) {
        out.emplace_back(prefix + ".A", scStr(c.key_A));
        out.emplace_back(prefix + ".B", scStr(c.key_B));
        out.emplace_back(prefix + ".TurboA", scStr(c.key_TurboA));
        out.emplace_back(prefix + ".TurboB", scStr(c.key_TurboB));
        out.emplace_back(prefix + ".Select", scStr(c.key_Select));
        out.emplace_back(prefix + ".Start", scStr(c.key_Start));
        out.emplace_back(prefix + ".Up", scStr(c.key_Up));
        out.emplace_back(prefix + ".Down", scStr(c.key_Down));
        out.emplace_back(prefix + ".Left", scStr(c.key_Left));
        out.emplace_back(prefix + ".Right", scStr(c.key_Right));
    }

    static void readController(
        const std::unordered_map<std::string, std::string> &kv,
        const std::string &prefix, ControllerSettings &c) {
        auto get = [&](const std::string &suffix, SDL_Scancode &dst) {
            auto it = kv.find(prefix + suffix);
            if (it != kv.end())
                dst = parseScancode(it->second);
        };
        get(".A", c.key_A);
        get(".B", c.key_B);
        get(".TurboA", c.key_TurboA);
        get(".TurboB", c.key_TurboB);
        get(".Select", c.key_Select);
        get(".Start", c.key_Start);
        get(".Up", c.key_Up);
        get(".Down", c.key_Down);
        get(".Left", c.key_Left);
        get(".Right", c.key_Right);
    }

    std::vector<std::pair<std::string, std::string>> buildMap() const {
        std::vector<std::pair<std::string, std::string>> out;
        out.emplace_back("allowScanlineSync", std::to_string(allowScanlineSync));
        out.emplace_back("vsync", std::to_string(vsync));
        out.emplace_back("matchRefreshRate", std::to_string(matchRefreshRate));
        out.emplace_back("scanlineBufferMs", std::to_string(scanlineBufferMs));
        out.emplace_back("languageIndex", std::to_string(languageIndex));
        out.emplace_back("audio.volume", std::to_string(audioSettings.volume));
        out.emplace_back("audio.filter90", std::to_string(audioSettings.useFilter90));
        out.emplace_back("audio.filter440", std::to_string(audioSettings.useFilter440));
        out.emplace_back("audio.filter14k", std::to_string(audioSettings.useFilter14k));
        writeController(out, "pad0", controllers[0]);
        writeController(out, "pad1", controllers[1]);
        out.emplace_back("key.pause", chordStr(keys.pause));
        out.emplace_back("key.fullScreen", chordStr(keys.fullScreen));
        out.emplace_back("key.speedUp", chordStr(keys.speedUp));
        out.emplace_back("key.speedDown", chordStr(keys.speedDown));
        out.emplace_back("key.reset", chordStr(keys.reset));
        out.emplace_back("key.nsfTogglePause", chordStr(keys.nsfTogglePause));
        out.emplace_back("key.nsfNextSong", chordStr(keys.nsfNextSong));
        out.emplace_back("key.nsfPrevSong", chordStr(keys.nsfPrevSong));
        return out;
    }

    void applyMap(const std::unordered_map<std::string, std::string> &kv) {
        auto getBool = [&](const std::string &k, bool &dst) {
            auto it = kv.find(k);
            if (it != kv.end())
                dst = parse<int>(it->second, dst ? 1 : 0) != 0;
        };
        auto getInt = [&](const std::string &k, int &dst) {
            auto it = kv.find(k);
            if (it != kv.end())
                dst = parse<int>(it->second, dst);
        };
        auto getFloat = [&](const std::string &k, float &dst) {
            auto it = kv.find(k);
            if (it != kv.end())
                dst = parse<float>(it->second, dst);
        };
        auto getKey = [&](const std::string &k, SDL_Scancode &dst) {
            auto it = kv.find(k);
            if (it != kv.end())
                dst = parseScancode(it->second);
        };
        auto getChord = [&](const std::string &k, KeyChord &dst) {
            auto it = kv.find(k);
            if (it != kv.end())
                dst = parseChord(it->second);
        };

        getBool("allowScanlineSync", allowScanlineSync);
        getBool("vsync", vsync);
        getBool("matchRefreshRate", matchRefreshRate);
        getInt("scanlineBufferMs", scanlineBufferMs);
        getInt("languageIndex", languageIndex);
        getFloat("audio.volume", audioSettings.volume);
        getBool("audio.filter90", audioSettings.useFilter90);
        getBool("audio.filter440", audioSettings.useFilter440);
        getBool("audio.filter14k", audioSettings.useFilter14k);
        readController(kv, "pad0", controllers[0]);
        readController(kv, "pad1", controllers[1]);
        getChord("key.pause", keys.pause);
        getChord("key.fullScreen", keys.fullScreen);
        getChord("key.speedUp", keys.speedUp);
        getChord("key.speedDown", keys.speedDown);
        getChord("key.reset", keys.reset);
        getChord("key.nsfTogglePause", keys.nsfTogglePause);
        getChord("key.nsfNextSong", keys.nsfNextSong);
        getChord("key.nsfPrevSong", keys.nsfPrevSong);
    }
};
