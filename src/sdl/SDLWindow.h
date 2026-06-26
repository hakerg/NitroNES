#pragma once
#include "../IFileSession.h"
#include "../IInputContext.h"
#include "../IWindow.h"
#include "../core/NESConst.h"
#include "ISDLWindowAPI.h"
#include "imgui/ImGuiLayer.h"

#include <SDL3/SDL.h>
#include <memory>
#include <stdexcept>
#include <string>

class SDLWindow : public IWindow {
public:
    SDLWindow(ISDLWindowAPI &hardwareAPI) : platformAPI(hardwareAPI) {
        // --- Inicjalizacja SDL ---
        if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
            throw std::runtime_error(std::string("SDL_Init: ") +
                                     SDL_GetError());

        int winW = (NES::SCREEN_WIDTH * NES::PAR_NUM * 3) / NES::PAR_DEN;
        int winH = NES::VISIBLE_H * 3;

        window =
            SDL_CreateWindow("NES Emulator", winW, winH, SDL_WINDOW_RESIZABLE);
        if (!window)
            throw std::runtime_error(std::string("SDL_CreateWindow: ") +
                                     SDL_GetError());

        renderer = SDL_CreateRenderer(window, nullptr);
        if (!renderer)
            throw std::runtime_error(std::string("SDL_CreateRenderer: ") +
                                     SDL_GetError());
        SDL_SetRenderVSync(renderer, 0);

        texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_BGRA32,
                                    SDL_TEXTUREACCESS_STREAMING,
                                    NES::SCREEN_WIDTH, NES::SCREEN_HEIGHT);
        if (!texture)
            throw std::runtime_error(std::string("SDL_CreateTexture: ") +
                                     SDL_GetError());
        SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
        SDL_SetWindowFullscreen(window, false);

        // --- Inicjalizacja ImGui (przez osobną warstwę) ---
        uiLayer = std::make_unique<ImGuiLayer>(window, renderer);
    }

    ~SDLWindow() override {
        // uiLayer zostanie zniszczone automatycznie tutaj (przed SDL), dzięki
        // unique_ptr
        if (texture)
            SDL_DestroyTexture(texture);
        if (renderer)
            SDL_DestroyRenderer(renderer);
        if (window)
            SDL_DestroyWindow(window);
        SDL_Quit();
    }

    // --- Obsługa zdarzeń ---
    bool pollEvent(AppEvent &out) override {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (uiLayer->processEvent(&ev))
                continue;
            if (toAppEvent(ev, out))
                return true;
        }
        return false;
    }

    void showCursor(bool visible) override {
        if (visible)
            SDL_ShowCursor();
        else
            SDL_HideCursor();
    }

    bool isFullscreen() const override {
        return (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != 0;
    }

    void toggleFullscreen() override {
        bool isFs = isFullscreen();
        SDL_SetWindowFullscreen(window, !isFs);
    }

    void setTitle(const std::string &title) override {
        SDL_SetWindowTitle(window, title.c_str());
    }

    uint64_t getTicks() const override { return SDL_GetTicks(); }
    void delay(uint32_t ms) override { SDL_Delay(ms); }

    void getPixelSize(int &w, int &h) const {
        SDL_GetWindowSizeInPixels(window, &w, &h);
    }

    // --- Renderowanie ---
    void presentNESFrame(const uint32_t *framebuffer,
                         IFileSession &session, double baseSpeed) override {
        SDL_UpdateTexture(texture, nullptr, framebuffer,
                          NES::SCREEN_WIDTH * sizeof(uint32_t));
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        int w = 0, h = 0;
        getPixelSize(w, h);

        float dstX = 0, dstY = 0, dstW = 0, dstH = 0;
        NES::calcDestRect(w, h, dstX, dstY, dstW, dstH);

        SDL_FRect srcRect = {0.0f, static_cast<float>(NES::OVERSCAN_TOP),
                             static_cast<float>(NES::SCREEN_WIDTH),
                             static_cast<float>(NES::VISIBLE_H)};
        SDL_FRect dstRect = {dstX, dstY, dstW, dstH};
        SDL_RenderTexture(renderer, texture, &srcRect, &dstRect);

        uiLayer->render(renderer, &session, baseSpeed);
        SDL_RenderPresent(renderer);
    }

    void presentBlank(IFileSession &session, double baseSpeed) override {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        uiLayer->render(renderer, &session, baseSpeed);
        SDL_RenderPresent(renderer);
    }

    void presentBlank(double baseSpeed) override {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        uiLayer->render(renderer, nullptr, baseSpeed);
        SDL_RenderPresent(renderer);
    }

    // --- GUI (Menu) ---
    void initMenu(AppSettings &s, IMenuHandler &h,
                  IInputContext &input) override {
        this->input = &input;
        uiLayer->init(&s, &h, &input);
    }
    bool isMenuOpen() const override { return uiLayer->isMenuOpen(); }
    std::string openFileDialog() override {
        return platformAPI.openFileDialog(window);
    }

    // --- Sprzęt i Synchronizacja ---
    bool getScanLine(int &outRaw) const override {
        return platformAPI.getScanLine(window, outRaw);
    }

    double getRefreshHz() const override {
        SDL_DisplayID displayID = SDL_GetDisplayForWindow(window);
        if (displayID == 0)
            return 60.0;

        const SDL_DisplayMode *mode = SDL_GetCurrentDisplayMode(displayID);
        if (mode && mode->refresh_rate_denominator > 0) {
            return (double)mode->refresh_rate_numerator /
                   (double)mode->refresh_rate_denominator;
        }
        return 60.0;
    }

    void getMonitorGeometry(int &w, int &h) const override {
        w = 0;
        h = 0;
        SDL_DisplayID displayID = SDL_GetDisplayForWindow(window);
        if (displayID == 0)
            return;

        SDL_Rect bounds;
        if (!SDL_GetDisplayBounds(displayID, &bounds))
            return;

        w = bounds.w;
        h = bounds.h;
    }

private:
    AppKey scancodeToAppKey(SDL_Scancode sc, SDL_Keymod mods) const {
        if (!input)
            return AppKey::Unknown;
        constexpr AppKey keys[] = {
            AppKey::Pause,       AppKey::FullScreen,  AppKey::SpeedUp,
            AppKey::SpeedDown,   AppKey::Reset,       AppKey::NsfTogglePause,
            AppKey::NsfNextSong, AppKey::NsfPrevSong,
        };
        for (AppKey key : keys) {
            if (input->appKeyBinding(key).matches(sc, mods))
                return key;
        }
        return AppKey::Unknown;
    }

    AppKey appKeyForRelease(SDL_Scancode sc) const {
        if (!input)
            return AppKey::Unknown;
        constexpr AppKey keys[] = {
            AppKey::Pause,       AppKey::FullScreen,  AppKey::SpeedUp,
            AppKey::SpeedDown,   AppKey::Reset,       AppKey::NsfTogglePause,
            AppKey::NsfNextSong, AppKey::NsfPrevSong,
        };
        uint8_t modBit = KeyChord::modBitForScancode(sc);
        for (AppKey key : keys) {
            KeyChord c = input->appKeyBinding(key);
            if (c.empty())
                continue;
            if (c.scancode == sc)
                return key;
            if (modBit && (c.mods & modBit))
                return key;
        }
        return AppKey::Unknown;
    }

    bool toAppEvent(const SDL_Event &ev, AppEvent &out) {
        switch (ev.type) {
        case SDL_EVENT_QUIT:
            out = {AppEventType::Quit};
            return true;
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
            out = {AppEventType::WindowResized};
            return true;
        case SDL_EVENT_MOUSE_MOTION:
            out = {AppEventType::MouseMoved};
            return true;
        case SDL_EVENT_KEY_DOWN:
            if (ev.key.repeat)
                return false;
            if (input && input->inputBlocked())
                return false;
            out = {AppEventType::KeyDown,
                   scancodeToAppKey(ev.key.scancode, ev.key.mod)};
            return out.key != AppKey::Unknown;
        case SDL_EVENT_KEY_UP: {
            if (input && input->inputBlocked())
                return false;
            AppKey k = appKeyForRelease(ev.key.scancode);
            if (k == AppKey::Unknown)
                return false;
            out = {AppEventType::KeyUp, k};
            return true;
        }
        case SDL_EVENT_GAMEPAD_ADDED:
            out.type = AppEventType::GamepadAdded;
            out.deviceId = ev.gdevice.which;
            return true;
        case SDL_EVENT_GAMEPAD_REMOVED:
            out.type = AppEventType::GamepadRemoved;
            out.deviceId = ev.gdevice.which;
            return true;
        case SDL_EVENT_GAMEPAD_AXIS_MOTION: {
            constexpr Sint16 THRESHOLD = 8000;
            if (ev.gaxis.axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) {
                out.type = AppEventType::GamepadAxisRightTrigger;
                out.axisDown = ev.gaxis.value > THRESHOLD;
                out.deviceId = ev.gaxis.which;
                return true;
            }
            if (ev.gaxis.axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER) {
                out.type = AppEventType::GamepadAxisLeftTrigger;
                out.axisDown = ev.gaxis.value > THRESHOLD;
                out.deviceId = ev.gaxis.which;
                return true;
            }
            return false;
        }
        default:
            return false;
        }
    }

    SDL_Window *window = nullptr;
    SDL_Renderer *renderer = nullptr;
    SDL_Texture *texture = nullptr;
    ISDLWindowAPI &platformAPI;
    IInputContext *input = nullptr;

    std::unique_ptr<ImGuiLayer> uiLayer;
};