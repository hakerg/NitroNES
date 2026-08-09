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
            {"file.open", "Open"},
            {"file.reload", "Reload"},
            {"file.close", "Close"},
            {"file.quit", "Quit"},
            {"file.saveState", "Quick Save"},
            {"file.loadState", "Quick Load"},
            {"emulation", "Emulation"},
            {"emulation.system", "System"},
            {"emulation.pause", "Pause"},
            {"emulation.reset", "Reset"},
            {"settings", "Settings"},
            {"tools", "Tools"},
            {"tools.about_file", "About File"},
            {"tools.memory_viewer", "Memory Viewer"},
            {"tools.memory_viewer.address", "Address"},
            {"settings.language", "Language"},
            {"settings.sync", "Synchronization"},
            {"settings.vsync", "Vertical sync"},
            {"settings.speed", "Base speed"},
            {"settings.speed1", "Speed up"},
            {"settings.speed2", "Speed down"},
            {"settings.sync_mode", "Adjust emulation speed"},
            {"settings.sync.none", "Standard"},
            {"settings.sync.none.tooltip", "Runs the emulation at the console's original native speed."},
            {"settings.sync.refresh_rate", "Monitor refresh rate"},
            {"settings.sync.refresh_rate.tooltip", "Requires a monitor with a refresh rate close to 60Hz (NTSC) / 50Hz (PAL) or their multiples."},
            {"settings.sync.scanline", "Minimal video lag"},
            {"settings.sync.scanline.tooltip", "Synchronizes emulation with the monitor beam to minimize input lag.\nRequires fullscreen mode and a compatible monitor refresh rate.\nAutomatically disables VSync."},
            {"settings.target_speed", "Target speed"},
            {"settings.scanline.buffer", "Buffer [ms]"},
            {"settings.scanline.buffer.tooltip", "Buffer for the 'Minimal video lag' mode.\nWARNING: If set too low (e.g., 1ms), the emulator falls a full frame behind.\nThis INCREASES lag while looking perfectly smooth.\nTo calibrate: lower the value until you see screen tearing, then increase it slightly until the tearing stops."},
            {"settings.audio", "Audio"},
            {"settings.volume", "Volume"},
            {"settings.audio.use_filters", "Use original filters"},
            {"settings.audio.adjust_pitch", "Adjust pitch with speed"},
            {"settings.controls", "Controls"},
            {"settings.graphics", "Graphics"},
            {"settings.graphics.use_backdrop", "Use game backdrop color as window background"},
            {"settings.graphics.preserve_aspect", "Preserve aspect ratio"},
            {"controls.title", "Controls"},
            {"controls.pad1", "Pad 1"},
            {"controls.pad2", "Pad 2"},
            {"controls.emulation", "General"},
            {"controls.nsf", "NSF player"},
            {"controls.up", "Up"},
            {"controls.down", "Down"},
            {"controls.left", "Left"},
            {"controls.right", "Right"},
            {"controls.pause", "Pause"},
            {"controls.fullscreen", "Fullscreen"},
            {"controls.speedup", "Speed up"},
            {"controls.speeddown", "Speed down"},
            {"controls.reset", "Reset"},
            {"controls.open", "Open file"},
            {"controls.reload", "Reload file"},
            {"controls.nsf.pause", "Pause"},
            {"controls.nsf.next", "Next track"},
            {"controls.nsf.prev", "Previous track"},
            {"controls.savestate", "Quick save/load"},
            {"controls.bind_all", "Bind all"},
            {"controls.clear", "Clear"},
            {"controls.clear_all", "Clear all"},
            {"controls.action", "Action"},
            {"controls.key", "Key"},
            {"controls.press_key", "Press key"},
            {"controls.set", "Set"},
            {"controls.cancel", "Cancel"}};
        auto it = dict.find(id);
        return it != dict.end() ? it->second : id;
    }
};