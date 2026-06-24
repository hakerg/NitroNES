#pragma once
#include <cstdint>

enum class AppKey {
    Unknown,
    Pause,
    FullScreen,
    SpeedUp,
    SpeedDown,
    Reset,
    NsfTogglePause,
    NsfNextSong,
    NsfPrevSong,
};

enum class AppEventType {
    Quit,
    WindowResized,
    KeyDown,
    KeyUp,
    GamepadAdded,
    GamepadRemoved,
    GamepadAxisRightTrigger,
    GamepadAxisLeftTrigger,
    MouseMoved,
};

struct AppEvent {
    AppEventType type;
    AppKey key = AppKey::Unknown; // KeyDown / KeyUp
    bool axisDown = false;        // GamepadAxis*
    uint32_t deviceId = 0;        // Gamepad*
};
