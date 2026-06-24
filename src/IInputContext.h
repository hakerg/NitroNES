#pragma once
#include "AppEvent.h"
#include "KeyChord.h"
#include "sdl/ControllerSettings.h"
#include <SDL3/SDL.h>
#include <cstdint>

class IInputContext {
public:
    virtual ~IInputContext() = default;

    // Odczyt stanu kontrolera NES (port 0 lub 1)
    virtual uint8_t readController(int port) const = 0;

    // Wywołanie na koniec każdej klatki (turbo tick)
    virtual void tickFrame() = 0;

    // Gamepad hotplug
    virtual void onGamepadAdded(uint32_t deviceId) = 0;
    virtual void onGamepadRemoved(uint32_t deviceId) = 0;

    // Trigger analogowy (szybkość)
    virtual void onRightTrigger(bool pressed) = 0;
    virtual void onLeftTrigger(bool pressed) = 0;

    virtual ControllerSettings &controllerSettings(int port) = 0;
    virtual const ControllerSettings &controllerSettings(int port) const = 0;
    virtual KeyChord appKeyBinding(AppKey key) const = 0;
    virtual void setAppKeyBinding(AppKey key, KeyChord chord) = 0;

    virtual void setInputBlocked(bool blocked) = 0;
    virtual bool inputBlocked() const = 0;
};
