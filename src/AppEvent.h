#pragma once
#include <cstdint>

enum class AppKey {
    Unknown,
    Pause,
    FullScreen,
    SpeedUp,
    SpeedDown,
    Reset,
    Open,
    NsfTogglePause,
    NsfNextSong,
    NsfPrevSong,
    Reload,
    SaveState0, SaveState1, SaveState2, SaveState3, SaveState4,
    SaveState5, SaveState6, SaveState7, SaveState8, SaveState9,
    LoadState0, LoadState1, LoadState2, LoadState3, LoadState4,
    LoadState5, LoadState6, LoadState7, LoadState8, LoadState9,
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
    MouseButtonDown
};

struct AppEvent {
    AppEventType type;
    AppKey key = AppKey::Unknown; // KeyDown / KeyUp
    bool axisDown = false;        // GamepadAxis*
    uint32_t deviceId = 0;        // Gamepad*
};
