#pragma once
#include <SDL3/SDL.h>
#include <cstdint>
#include <string>

struct KeyChord {
    SDL_Scancode scancode = SDL_SCANCODE_UNKNOWN;
    uint8_t mods = 0;

    static constexpr uint8_t MOD_CTRL = 1;
    static constexpr uint8_t MOD_SHIFT = 2;
    static constexpr uint8_t MOD_ALT = 4;

    bool empty() const { return scancode == SDL_SCANCODE_UNKNOWN; }

    bool operator==(const KeyChord &o) const {
        return scancode == o.scancode && mods == o.mods;
    }

    static uint8_t modsFromSDL(SDL_Keymod m) {
        uint8_t r = 0;
        if (m & SDL_KMOD_CTRL)
            r |= MOD_CTRL;
        if (m & SDL_KMOD_SHIFT)
            r |= MOD_SHIFT;
        if (m & SDL_KMOD_ALT)
            r |= MOD_ALT;
        return r;
    }

    static bool isModifierScancode(SDL_Scancode sc) {
        return modBitForScancode(sc) != 0;
    }

    static uint8_t modBitForScancode(SDL_Scancode sc) {
        switch (sc) {
        case SDL_SCANCODE_LCTRL:
        case SDL_SCANCODE_RCTRL:
            return MOD_CTRL;
        case SDL_SCANCODE_LSHIFT:
        case SDL_SCANCODE_RSHIFT:
            return MOD_SHIFT;
        case SDL_SCANCODE_LALT:
        case SDL_SCANCODE_RALT:
            return MOD_ALT;
        default:
            return 0;
        }
    }

    bool matches(SDL_Scancode pressed, SDL_Keymod activeMods) const {
        if (empty() || pressed != scancode)
            return false;
        uint8_t m = modsFromSDL(activeMods) & ~modBitForScancode(scancode);
        return m == mods;
    }

    bool active() const {
        if (empty())
            return false;
        int n = 0;
        const bool *ks = SDL_GetKeyboardState(&n);
        if (!ks || scancode >= (SDL_Scancode)n || !ks[scancode])
            return false;
        uint8_t m =
            modsFromSDL(SDL_GetModState()) & ~modBitForScancode(scancode);
        return m == mods;
    }

    std::string name() const {
        if (empty())
            return "-";
        std::string s;
        if (mods & MOD_CTRL)
            s += "Ctrl+";
        if (mods & MOD_SHIFT)
            s += "Shift+";
        if (mods & MOD_ALT)
            s += "Alt+";
        const char *kn = SDL_GetScancodeName(scancode);
        s += (kn && kn[0]) ? kn : "?";
        return s;
    }
};


