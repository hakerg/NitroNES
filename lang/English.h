#pragma once
#include "ILanguage.h"
#include <string>
#include <unordered_map>

class English : public ILanguage {
public:
    const char *getName() const override { return "English"; }

    const char *tr(const char *id) const override {
        static const std::unordered_map<std::string, const char *> dict = {
            {"file", "File"},
            {"file.open", "Open..."},
            {"file.reload", "Reload"},
            {"file.quit", "Quit"},
            {"emulation", "Emulation"},
            {"emulation.system", "Subsystem"},
            {"emulation.pause", "Pause"},
            {"emulation.reset", "Reset"},
            {"settings", "Settings"},
            {"settings.language", "Language"},
            {"settings.sync", "Synchronization"},
            {"settings.vsync", "Vertical sync"},
            {"settings.match_hz", "Match monitor refresh rate"},
            {"settings.scanline", "Scanline sync"},
            {"settings.scanline.enabled", "Enabled"},
            {"settings.scanline.buffer", "Buffer [ms]"},
            {"settings.audio", "Audio"},
            {"settings.volume", "Volume"},
            {"settings.audio.filters", "Filters"},
            {"settings.audio.hp90", "High-pass filter 90 Hz"},
            {"settings.audio.hp440", "High-pass filter 440 Hz"},
            {"settings.audio.lp14k", "Low-pass filter 14 kHz"},
            {"settings.controls", "Controls..."},
            {"controls.title", "Controls"},
            {"controls.pad1", "Pad 1"},
            {"controls.pad2", "Pad 2"},
            {"controls.emulation", "Emulation"},
            {"controls.nsf", "NSF"},
            {"controls.up", "Up"},
            {"controls.down", "Down"},
            {"controls.left", "Left"},
            {"controls.right", "Right"},
            {"controls.pause", "Pause"},
            {"controls.fullscreen", "Fullscreen"},
            {"controls.speedup", "Speed up"},
            {"controls.speeddown", "Speed down"},
            {"controls.reset", "Reset"},
            {"controls.nsf.pause", "Pause"},
            {"controls.nsf.next", "Next track"},
            {"controls.nsf.prev", "Previous track"},
            {"controls.bind_all", "Bind all"},
            {"controls.clear", "Clear"},
            {"controls.clear_all", "Clear all"},
            {"controls.action", "Action"},
            {"controls.key", "Key"},
            {"controls.press_key", "Press key..."},
            {"controls.set", "Set"},
            {"controls.cancel", "Cancel"}};
        auto it = dict.find(id);
        return it != dict.end() ? it->second : id;
    }
};