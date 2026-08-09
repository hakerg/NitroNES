#pragma once
#include "../AppSettings.h"
#include "../IInputContext.h"
#include "ControllerSettings.h"
#include "NESController.h"
#include <SDL3/SDL.h>

class SDLInputContext : public IInputContext {
public:
    explicit SDLInputContext(AppSettings &settings)
        : settings(settings),
          controller1(settings.controllers[0]),
          controller2(settings.controllers[1]) {}

    uint8_t readController(int port) const override {
        if (blocked)
            return 0;
        return port == 0 ? controller1.readState() : controller2.readState();
    }

    void tickFrame() override {
        if (blocked)
            return;
        controller1.tickFrame();
        controller2.tickFrame();
    }

    void onGamepadAdded(uint32_t deviceId) override {
        if (!controller1.gamepad()) {
            if (SDL_Gamepad *gp = SDL_OpenGamepad(deviceId))
                controller1.attachGamepad(gp);
        } else if (!controller2.gamepad()) {
            if (SDL_Gamepad *gp = SDL_OpenGamepad(deviceId))
                controller2.attachGamepad(gp);
        }
    }

    void onGamepadRemoved(uint32_t deviceId) override {
        if (controller1.gamepadID() == deviceId) {
            controller1.closeAndDetachGamepad();
            return;
        }
        if (controller2.gamepadID() == deviceId) {
            controller2.closeAndDetachGamepad();
        }
    }

    void onRightTrigger(bool pressed) override { padFast = pressed; }
    void onLeftTrigger(bool pressed) override { padSlow = pressed; }

    bool isFast() const { return padFast; }
    bool isSlow() const { return padSlow; }

    ControllerSettings &controllerSettings(int port) override {
        return settings.controllers[port == 0 ? 0 : 1];
    }

    const ControllerSettings &controllerSettings(int port) const override {
        return settings.controllers[port == 0 ? 0 : 1];
    }

    KeyChord appKeyBinding(AppKey key) const override {
        switch (key) {
        case AppKey::Pause: return settings.keys.pause;
        case AppKey::FullScreen: return settings.keys.fullScreen;
        case AppKey::SpeedUp: return settings.keys.speedUp;
        case AppKey::SpeedDown: return settings.keys.speedDown;
        case AppKey::Reset: return settings.keys.reset;
        case AppKey::Open: return settings.keys.open;
        case AppKey::NsfTogglePause: return settings.keys.nsfTogglePause;
        case AppKey::NsfNextSong: return settings.keys.nsfNextSong;
        case AppKey::NsfPrevSong: return settings.keys.nsfPrevSong;
        case AppKey::Reload: return settings.keys.reload;
        case AppKey::Rewind: return settings.keys.rewind;
        default: {
            int s = saveStateSlot(key);
            if (s >= 0) return settings.keys.saveState[s];
            int l = loadStateSlot(key);
            if (l >= 0) return settings.keys.loadState[l];
            return {};
        }
        }
    }

    void setAppKeyBinding(AppKey key, KeyChord chord) override {
        switch (key) {
        case AppKey::Pause: settings.keys.pause = chord; return;
        case AppKey::FullScreen: settings.keys.fullScreen = chord; return;
        case AppKey::SpeedUp: settings.keys.speedUp = chord; return;
        case AppKey::SpeedDown: settings.keys.speedDown = chord; return;
        case AppKey::Reset: settings.keys.reset = chord; return;
        case AppKey::Open: settings.keys.open = chord; return;
        case AppKey::NsfTogglePause:
            settings.keys.nsfTogglePause = chord;
            return;
        case AppKey::NsfNextSong: settings.keys.nsfNextSong = chord; return;
        case AppKey::NsfPrevSong: settings.keys.nsfPrevSong = chord; return;
        case AppKey::Reload: settings.keys.reload = chord; return;
        case AppKey::Rewind: settings.keys.rewind = chord; return;
        default: {
            int s = saveStateSlot(key);
            if (s >= 0) { settings.keys.saveState[s] = chord; return; }
            int l = loadStateSlot(key);
            if (l >= 0) { settings.keys.loadState[l] = chord; return; }
            return;
        }
        }
    }

    static int saveStateSlot(AppKey key) {
        int v = static_cast<int>(key);
        int base = static_cast<int>(AppKey::SaveState0);
        return (v >= base && v < base + 10) ? v - base : -1;
    }
    static int loadStateSlot(AppKey key) {
        int v = static_cast<int>(key);
        int base = static_cast<int>(AppKey::LoadState0);
        return (v >= base && v < base + 10) ? v - base : -1;
    }

private:
    AppSettings &settings;
    NESController controller1;
    NESController controller2;
    bool padFast = false;
    bool padSlow = false;
    bool blocked = false;

public:
    void setInputBlocked(bool b) override { blocked = b; }
    bool inputBlocked() const override { return blocked; }
};
