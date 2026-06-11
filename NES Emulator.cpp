#include "App.h"
#include "AppSettings.h"
#include "sdl/SDLAudioStream.h"
#include "sdl/SDLInputContext.h"
#include "sdl/SDLWindow.h"
#include "sdl/windows/WindowAPI.h"
#include <SDL3/SDL.h>
#include <windows.h>
#include <iostream>
#include <string>
#include <filesystem>

#pragma comment(lib, "winmm.lib")

int wmain(int argc, wchar_t* argv[]) {
    SetConsoleOutputCP(CP_UTF8);
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    if (argc < 2) {
        std::cerr << "Uzycie: NESEmulator <plik.nsf | plik.nes>\n";
        return 1;
    }

    std::wstring wPath = argv[1];
    auto u8 = std::filesystem::path(wPath).u8string();
    std::string romPath(u8.begin(), u8.end());

    try {
        WindowAPI platformAPI;
        SDLWindow window(&platformAPI);
        AppSettings settings;
        SDLAudioStream audio(settings.audioSettings);
        SDLInputContext input;

        App app(romPath, window, input, audio, settings);
        app.run();
    }
    catch (const std::exception& e) {
        std::cerr << "Blad: " << e.what() << "\n";
        return 1;
    }

    return 0;
}