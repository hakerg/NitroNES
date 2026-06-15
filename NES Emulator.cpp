#include "App.h"
#include "AppSettings.h"
#include "sdl/SDLAudioStream.h"
#include "sdl/SDLInputContext.h"
#include "sdl/SDLWindow.h"
#include "sdl/windows/WindowAPI.h"
#include <SDL3/SDL.h>
#include <windows.h>
#include <shellapi.h>
#include <string>
#include <filesystem>

#pragma comment(lib, "winmm.lib")

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    int argc;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);

    if (argc < 2) {
        MessageBoxW(NULL, L"Usage: NESEmulator <file.nsf | file.nes>", L"Error", MB_OK | MB_ICONERROR);
        LocalFree(argv);
        return 1;
    }

    std::wstring wPath = argv[1];
    LocalFree(argv);

    auto u8 = std::filesystem::path(wPath).u8string();
    std::string romPath(u8.begin(), u8.end());

    try {
        WindowAPI platformAPI;
        SDLWindow window(platformAPI);
        AppSettings settings;
        SDLAudioStream audio(settings.audioSettings);
        SDLInputContext input;

        App app(romPath, window, input, audio, settings);
        app.run();
    }
    catch (const std::exception& e) {
        std::string msg = std::string("Error: ") + e.what();
        MessageBoxA(NULL, msg.c_str(), "Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    return 0;
}