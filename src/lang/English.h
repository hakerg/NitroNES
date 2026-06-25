#pragma once
#include "ILanguage.h"
#include <string>
#include <unordered_map>

class English : public ILanguage {
public:
    const char *getName() const override { return "English"; }
    const char *getCode() const override { return "en"; }

    const char *tr(const char *id) const override {
        static const std::unordered_map<std::string, const char *> dict = {
            {"file", "File"},
            {"file.open", "Open..."},
            {"file.reload", "Reload"},
            {"file.close", "Close"},
            {"file.quit", "Quit"},
            {"emulation", "Emulation"},
            {"emulation.system", "Subsystem"},
            {"emulation.pause", "Pause"},
            {"emulation.reset", "Reset"},
            {"settings", "Settings"},
            {"settings.language", "Language"},
            {"settings.sync", "Synchronization..."},
            {"settings.vsync", "Vertical sync"},
            {"settings.sync_mode", "Synchronization mode"},
            {"settings.sync.none", "Standard"},
            {"settings.sync.timer", "Refresh rate"},
            {"settings.sync.scanline", "Beam racing"},
            {"settings.current_speed", "Current speed"},
            {"settings.scanline.buffer", "Buffer [ms]"},
            {"status.system_error", "Unavailable: system error"},
            {"status.outside_tolerance", "Refresh rate unsupported"},
            {"status.no_fullscreen", "Requires fullscreen"},
            {"settings.audio", "Audio..."},
            {"settings.volume", "Volume"},
            {"settings.audio.filters", "Filters"},
            {"settings.audio.hp90", "High-pass filter 90 Hz"},
            {"settings.audio.hp440", "High-pass filter 440 Hz"},
            {"settings.audio.lp14k", "Low-pass filter 14 kHz"},
            {"settings.audio.reduce_clicks", "Reduce artifacts"},
            {"settings.audio.adjust_pitch", "Adjust pitch with speed"},
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