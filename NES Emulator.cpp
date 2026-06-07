#include "NESConst.h"
#include "NESCoreBase.h"
#include "NSFPlayer.h"
#include "NESSystem.h"
#include "SDLAudioStream.h"
#include "SyncStrategy.h"
#include "TimerSyncStrategy.h"
#include "ScanlineSyncStrategy.h"
#include "ImGuiMenu.h"
#include "MonitorRefreshRateDetector.h"
#include <SDL3/SDL.h>
#include <iostream>
#include <fstream>
#include <string>
#include <filesystem>
#include <memory>
#include <cmath>
#include <thread>
#include <atomic>
#include <windows.h>
#pragma comment(lib, "winmm.lib")

static bool isNesRomFile(const std::string& path) {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) return false;
    char m[5] = {};
    ifs.read(m, 5);
    if (ifs.gcount() < 4) return false;
    if (m[0] == 'N' && m[1] == 'E' && m[2] == 'S' && m[3] == 'M') return false;
    return m[0] == 'N' && m[1] == 'E' && m[2] == 'S' && m[3] == 0x1A;
}

int wmain(int argc, wchar_t* argv[]) {
    SetConsoleOutputCP(CP_UTF8);

    if (argc < 2) {
        std::cerr << "Uzycie: NESEmulator <plik.nsf | plik.nes>\n";
        return 1;
    }

    std::wstring wPath = argv[1];
    for (int i = 2; i < argc; i++) { wPath += L' '; wPath += argv[i]; }

    auto u8 = std::filesystem::path(wPath).u8string();
    std::string romPath(u8.begin(), u8.end());
    std::string filename = std::filesystem::path(wPath).filename().string();

    std::unique_ptr<NESCoreBase> core = isNesRomFile(romPath)
        ? std::unique_ptr<NESCoreBase>(std::make_unique<NESSystem>())
        : std::unique_ptr<NESCoreBase>(std::make_unique<NSFPlayer>());

    if (!core->loadFile(romPath)) return 1;

    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
        std::cerr << "Blad SDL3: " << SDL_GetError() << "\n";
        return 1;
    }

    int winW = (NES::SCREEN_WIDTH * NES::PAR_NUM * 3) / NES::PAR_DEN;
    int winH = NES::VISIBLE_H * 3;

    SDL_Window* window = SDL_CreateWindow(core->windowTitle(filename).c_str(), winW, winH, SDL_WINDOW_RESIZABLE);
    if (!window) { std::cerr << "Blad okna: " << SDL_GetError() << "\n"; SDL_Quit(); return 1; }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    if (!renderer) {
        std::cerr << "Blad renderera: " << SDL_GetError() << "\n";
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_SetRenderVSync(renderer, 0);

    SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_BGRA32, SDL_TEXTUREACCESS_STREAMING, NES::SCREEN_WIDTH, NES::SCREEN_HEIGHT);
    if (!texture) {
        std::cerr << "Blad tekstury: " << SDL_GetError() << "\n";
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);

    bool allowScanlineSync = false;
    bool vsync = false;
    bool matchRefreshRate = true;
    int volume = 100;
    int scanlineBufferMs = 8;

    ImGuiMenu menu(window, renderer, core.get(),
                   &allowScanlineSync, &vsync, &matchRefreshRate, &volume, &scanlineBufferMs);
    bool guiActive = true;

    if (auto* nesSystem = dynamic_cast<NESSystem*>(core.get())) {
        nesSystem->onRenderFrame = [&](const uint32_t* fb) {
            if (!fb) return;

            SDL_UpdateTexture(texture, nullptr, fb, NES::SCREEN_WIDTH * sizeof(uint32_t));

            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderClear(renderer);

            int w = 0, h = 0;
            SDL_GetWindowSizeInPixels(window, &w, &h);

            float dstX = 0, dstY = 0, dstW = 0, dstH = 0;
            NES::calcDestRect(w, h, dstX, dstY, dstW, dstH);

            SDL_FRect srcRect = {
                0.0f,
                static_cast<float>(NES::OVERSCAN_TOP),
                static_cast<float>(NES::SCREEN_WIDTH),
                static_cast<float>(NES::VISIBLE_H)
            };

            SDL_FRect dstRect = { dstX, dstY, dstW, dstH };

            SDL_RenderTexture(renderer, texture, &srcRect, &dstRect);

            if (guiActive) menu.render();

            SDL_RenderPresent(renderer);
            };
    }

    SDLAudioStream audioStream;
    if (!audioStream.open()) std::cerr << "Ostrzezenie: brak audio\n";

    core->onAudioSample = [&](float sample, double dt) {
        audioStream.addNESSample(dt, sample * (volume / 100.0f));
        };

    bool running = true;
    bool keyFast = false;
    bool keySlow = false;
    bool padFast = false;
    bool padSlow = false;
    SDL_JoystickID speedPadId = 0;

    Uint64 lastMouseMoveTime = SDL_GetTicks();

    auto calcSpeed = [&]() -> double {
             if (keyFast || padFast) return 4.0;
        else if (keySlow || padSlow) return 0.5;
        else                         return 1.0;
        };

    auto processEvent = [&](const SDL_Event& ev) {
        if (guiActive) menu.processEvent(&ev);

        if (ev.type == SDL_EVENT_MOUSE_MOTION) {
            lastMouseMoveTime = SDL_GetTicks();
            if (!guiActive) {
                SDL_ShowCursor();
                guiActive = true;
            }
            return;
        }
        switch (ev.type) {
        case SDL_EVENT_QUIT:
            running = false;
            return;
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
            core->renderFrame();
            return;
        case SDL_EVENT_KEY_DOWN:
            if (ev.key.repeat) return;
            if (ev.key.scancode == SDL_SCANCODE_TAB) {
                bool shift = (SDL_GetModState() & SDL_KMOD_SHIFT) != 0;
                if (shift) { keySlow = true;  keyFast = false; }
                else { keyFast = true;  keySlow = false; }
                return;
            }
            switch (ev.key.scancode) {
            case SDL_SCANCODE_P:
                core->paused = !core->paused;
                return;
            case SDL_SCANCODE_F11: {
                bool isFs = (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != 0;
                SDL_SetWindowFullscreen(window, !isFs);
                return;
            }
            case SDL_SCANCODE_SPACE:  core->onSpacePressed();     return;
            case SDL_SCANCODE_RIGHT:  core->onRightPressed();     return;
            case SDL_SCANCODE_LEFT:   core->onLeftPressed();      return;
            default: return;
            }
        case SDL_EVENT_KEY_UP:
            if (ev.key.scancode == SDL_SCANCODE_TAB) { keyFast = false; keySlow = false; }
            return;
        case SDL_EVENT_GAMEPAD_AXIS_MOTION: {
            if (ev.gaxis.axis != SDL_GAMEPAD_AXIS_RIGHT_TRIGGER &&
                ev.gaxis.axis != SDL_GAMEPAD_AXIS_LEFT_TRIGGER) return;

            constexpr Sint16 THRESHOLD = 8000;
            SDL_JoystickID jid = ev.gaxis.which;
            if (speedPadId == 0) speedPadId = jid;
            if (speedPadId != jid) return;
            if (ev.gaxis.axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) padFast = ev.gaxis.value > THRESHOLD;
            if (ev.gaxis.axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER)  padSlow = ev.gaxis.value > THRESHOLD;
            if (!padFast && !padSlow) speedPadId = 0;
            return;
        }
        case SDL_EVENT_GAMEPAD_ADDED:
            core->onGamepadAdded(ev.gdevice.which);
            return;
        case SDL_EVENT_GAMEPAD_REMOVED:
            core->onGamepadRemoved(ev.gdevice.which);
            return;
        default: break;
        }
        };

    MonitorRefreshRateDetector detector;
    HWND hwnd = (HWND)SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
    detector.setWindow(hwnd);

    SyncContext syncContext(
        core.get(),
        window,
        &detector,
        calcSpeed,
        &matchRefreshRate
    );

    audioStream.bindContext(&syncContext);

    TimerSyncStrategy timerStrategy(syncContext);
    ScanlineSyncStrategy scanlineStrategy(syncContext, &scanlineBufferMs);

    while (running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            processEvent(ev);
        }

        if (core->hasPPU()) {
            SyncStrategy* activeStrategy = nullptr;
            activeStrategy = allowScanlineSync && scanlineStrategy.canUse() ? static_cast<SyncStrategy*>(&scanlineStrategy) : &timerStrategy;
            activeStrategy->run();
        }
        else {
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderClear(renderer);
            if (guiActive) menu.render();
            SDL_RenderPresent(renderer);
        }

        if (guiActive && !menu.isOpen() && SDL_GetTicks() - lastMouseMoveTime >= 1000) {
            SDL_HideCursor();
            guiActive = false;
        }

        SDL_Delay(1);
    }

    menu.shutdown();
    audioStream.close();
    core->shutdown();

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}