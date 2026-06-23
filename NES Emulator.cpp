#include "App.h"
#include "AppSettings.h"
#include "lang/LanguageRegistry.h"
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

    std::wstring wPath = (argc >= 2) ? argv[1] : L"";
    LocalFree(argv);

    auto u8 = std::filesystem::path(wPath).u8string();
    std::string romPath(u8.begin(), u8.end());

    try {
        WindowAPI platformAPI;
        SDLWindow window(platformAPI);
        AppSettings settings;
        LanguageRegistry::instance().bindIndex(&settings.languageIndex);
        SDLAudioStream audio(settings.audioSettings);
        SDLInputContext input(settings);

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