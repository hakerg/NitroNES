#pragma once
#include "KeyChord.h"
#include "core/AudioStream.h"
#include "lang/LanguageRegistry.h"
#include "sdl/ControllerSettings.h"
#include <SDL3/SDL.h>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

struct AppKeyBindings {
    KeyChord pause{SDL_SCANCODE_P};
    KeyChord fullScreen{SDL_SCANCODE_F11};
    KeyChord speedUp{SDL_SCANCODE_TAB};
    KeyChord speedDown{SDL_SCANCODE_TAB, KeyChord::MOD_SHIFT};
    KeyChord reset{};
    KeyChord open{SDL_SCANCODE_O, KeyChord::MOD_CTRL};
    KeyChord nsfTogglePause{SDL_SCANCODE_SPACE};
    KeyChord nsfNextSong{SDL_SCANCODE_RIGHT};
    KeyChord nsfPrevSong{SDL_SCANCODE_LEFT};
};

struct AppSettings {
    bool vsync = false;
    int syncMode = 2;
    int scanlineBufferMs = 8;
    std::string language = "en";
    bool adjustPitch = true;
    float speed = 1.0f;
    float speed1 = 4.0f;
    float speed2 = 0.5f;
    AudioSettings audioSettings;
    ControllerSettings controllers[2] = {ControllerSettings::player1(),
                                         ControllerSettings::player2()};
    AppKeyBindings keys;

    AppSettings() {
        LanguageRegistry::instance().bindStorage(&language);
        load();
    }
    ~AppSettings() { save(); }

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

    static std::string toStr(const std::string &v) { return v; }
    static std::string toStr(bool v) { return v ? "1" : "0"; }
    static std::string toStr(int v) { return std::to_string(v); }
    static std::string toStr(float v) { return std::to_string(v); }
    static std::string toStr(SDL_Scancode v) { return std::to_string((int)v); }
    static std::string toStr(const KeyChord &c) {
        return std::to_string((int)c.scancode) + ":" + std::to_string(c.mods);
    }

    static void fromStr(const std::string &s, std::string &v) { v = s; }
    static void fromStr(const std::string &s, bool &v) { v = std::stoi(s) != 0; }
    static void fromStr(const std::string &s, int &v) { v = std::stoi(s); }
    static void fromStr(const std::string &s, float &v) { v = std::stof(s); }
    static void fromStr(const std::string &s, SDL_Scancode &v) {
        v = (SDL_Scancode)std::stoi(s);
    }
    static void fromStr(const std::string &s, KeyChord &c) {
        auto colon = s.find(':');
        c.scancode = (SDL_Scancode)std::stoi(s.substr(0, colon));
        c.mods = colon == std::string::npos
                     ? 0
                     : (uint8_t)std::stoi(s.substr(colon + 1));
    }

    template <class F> void each(F f) {
        f("vsync", vsync);
        f("syncMode", syncMode);
        f("scanlineBufferMs", scanlineBufferMs);
        f("language", language);
        f("audio.volume", audioSettings.volume);
        f("audio.filter90", audioSettings.useFilter90);
        f("audio.filter440", audioSettings.useFilter440);
        f("audio.filter14k", audioSettings.useFilter14k);
        f("audio.reduceClicks", audioSettings.reduceClicks);
        f("audio.adjustPitch", adjustPitch);
        f("speed", speed);
        f("speed1", speed1);
        f("speed2", speed2);
        for (int p = 0; p < 2; p++) {
            auto &c = controllers[p];
            std::string pre = "pad" + std::to_string(p) + ".";
            f(pre + "A", c.key_A);
            f(pre + "B", c.key_B);
            f(pre + "TurboA", c.key_TurboA);
            f(pre + "TurboB", c.key_TurboB);
            f(pre + "Select", c.key_Select);
            f(pre + "Start", c.key_Start);
            f(pre + "Up", c.key_Up);
            f(pre + "Down", c.key_Down);
            f(pre + "Left", c.key_Left);
            f(pre + "Right", c.key_Right);
        }
        f("key.pause", keys.pause);
        f("key.fullScreen", keys.fullScreen);
        f("key.speedUp", keys.speedUp);
        f("key.speedDown", keys.speedDown);
        f("key.reset", keys.reset);
        f("key.open", keys.open);
        f("key.nsfTogglePause", keys.nsfTogglePause);
        f("key.nsfNextSong", keys.nsfNextSong);
        f("key.nsfPrevSong", keys.nsfPrevSong);
    }

    void save() {
        std::ofstream out(filePath(), std::ios::trunc);
        if (!out)
            return;
        each([&](const std::string &k, auto &v) {
            out << k << '=' << toStr(v) << '\n';
        });
    }

    void load() {
        std::ifstream in(filePath());
        if (!in)
            return;
        std::string line;
        while (std::getline(in, line)) {
            auto eq = line.find('=');
            if (eq == std::string::npos)
                continue;
            std::string k = line.substr(0, eq);
            std::string val = line.substr(eq + 1);
            each([&](const std::string &fk, auto &fv) {
                if (fk == k) try { fromStr(val, fv); } catch (...) {}
            });
        }
    }
};
