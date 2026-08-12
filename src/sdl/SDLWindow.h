#pragma once
#include "../IFileSession.h"
#include "../IInputContext.h"
#include "../IWindow.h"
#include "../core/NESConst.h"
#include "../core/NESBus.h"
#include "ISDLWindowAPI.h"
#include "imgui/ImGuiLayer.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>
#include <memory>
#include <stdexcept>
#include <string>

class SDLWindow : public IWindow {
public:
    explicit SDLWindow(ISDLWindowAPI &hardwareAPI) : platformAPI(hardwareAPI) {
        if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
            throw std::runtime_error(std::string("SDL_Init: ") + SDL_GetError());

        int winW = (NES::SCREEN_WIDTH * NES::PAR_NUM * 3) / NES::PAR_DEN;
        int winH = NES::SCREEN_HEIGHT * 3;

        window = SDL_CreateWindow("NitroNES", winW, winH, SDL_WINDOW_RESIZABLE);
        if (!window)
            throw std::runtime_error(std::string("SDL_CreateWindow: ") + SDL_GetError());

        gpuDevice = SDL_CreateGPUDevice(
            SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_MSL | SDL_GPU_SHADERFORMAT_METALLIB,
            false, nullptr);
        if (!gpuDevice)
            throw std::runtime_error(std::string("SDL_CreateGPUDevice: ") + SDL_GetError());

        if (!SDL_ClaimWindowForGPUDevice(gpuDevice, window))
            throw std::runtime_error(std::string("SDL_ClaimWindowForGPUDevice: ") + SDL_GetError());

        if (!SDL_SetGPUAllowedFramesInFlight(gpuDevice, 1))
            throw std::runtime_error(std::string("SDL_SetGPUAllowedFramesInFlight: ") + SDL_GetError());
        if (!SDL_SetGPUSwapchainParameters(gpuDevice, window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, SDL_GPU_PRESENTMODE_IMMEDIATE))
            throw std::runtime_error(std::string("SDL_SetGPUSwapchainParameters: ") + SDL_GetError());

        SDL_GPUTextureCreateInfo texInfo = {};
        texInfo.type = SDL_GPU_TEXTURETYPE_2D;
        texInfo.format = SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM;
        texInfo.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
        texInfo.width = NES::SCREEN_WIDTH;
        texInfo.height = NES::SCREEN_HEIGHT;
        texInfo.layer_count_or_depth = 1;
        texInfo.num_levels = 1;
        texInfo.sample_count = SDL_GPU_SAMPLECOUNT_1;
        nesTexture = SDL_CreateGPUTexture(gpuDevice, &texInfo);
        if (!nesTexture)
            throw std::runtime_error(std::string("SDL_CreateGPUTexture: ") + SDL_GetError());

        SDL_GPUTransferBufferCreateInfo tbInfo = {};
        tbInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        tbInfo.size = NES::SCREEN_WIDTH * NES::SCREEN_HEIGHT * 4;
        nesTransfer = SDL_CreateGPUTransferBuffer(gpuDevice, &tbInfo);
        if (!nesTransfer)
            throw std::runtime_error(std::string("SDL_CreateGPUTransferBuffer: ") + SDL_GetError());

        uiLayer = std::make_unique<ImGuiLayer>(window, gpuDevice);
    }

    ~SDLWindow() override {
        uiLayer.reset();

        if (gpuDevice) {
            if (nesTexture) SDL_ReleaseGPUTexture(gpuDevice, nesTexture);
            if (nesTransfer) SDL_ReleaseGPUTransferBuffer(gpuDevice, nesTransfer);
            SDL_ReleaseWindowFromGPUDevice(gpuDevice, window);
            SDL_DestroyGPUDevice(gpuDevice);
        }
        if (window)
            SDL_DestroyWindow(window);
        SDL_Quit();
    }

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

    void pumpEvents() override { SDL_PumpEvents(); }

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

    void getPixelSize(int &w, int &h) const override {
        SDL_GetWindowSizeInPixels(window, &w, &h);
    }

    void presentNESFrame(IFileSession* session) override {
        SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(gpuDevice);
        if (!cmd)
            return;

        SDL_GPUTexture *swapchain = nullptr;
        Uint32 sw = 0, sh = 0;
        bool acquired = SDL_AcquireGPUSwapchainTexture(cmd, window, &swapchain, &sw, &sh);
        if (!acquired || !swapchain || sw == 0 || sh == 0) {
            SDL_CancelGPUCommandBuffer(cmd);
            return;
        }

        uint32_t *fb = session ? session->getFramebuffer() : nullptr;
        if (fb) {
            void *ptr = SDL_MapGPUTransferBuffer(gpuDevice, nesTransfer, true);
            if (!ptr) {
                SDL_CancelGPUCommandBuffer(cmd);
                return;
            }
            memcpy(ptr, fb, NES::SCREEN_WIDTH * NES::SCREEN_HEIGHT * sizeof(uint32_t));
            SDL_UnmapGPUTransferBuffer(gpuDevice, nesTransfer);

            SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);
            SDL_GPUTextureTransferInfo src = {};
            src.transfer_buffer = nesTransfer;
            SDL_GPUTextureRegion dst = {};
            dst.texture = nesTexture;
            dst.w = NES::SCREEN_WIDTH;
            dst.h = NES::SCREEN_HEIGHT;
            dst.d = 1;
            SDL_UploadToGPUTexture(copy, &src, &dst, true);
            SDL_EndGPUCopyPass(copy);

            float dstX = 0, dstY = 0, dstW = 0, dstH = 0;
            if (NESBus::instance().preserveAspectRatio) {
                NES::calcDestRect(sw, sh, dstX, dstY, dstW, dstH,
                                  session->getCore().system);
            } else {
                dstW = (float)sw;
                dstH = (float)sh;
            }
            SDL_GPUBlitInfo blit = {};
            blit.source.texture = nesTexture;
            blit.source.w = NES::SCREEN_WIDTH;
            blit.source.h = NES::SCREEN_HEIGHT;
            blit.destination.texture = swapchain;
            blit.destination.x = static_cast<int>(dstX);
            blit.destination.y = static_cast<int>(dstY);
            blit.destination.w = static_cast<int>(dstW);
            blit.destination.h = static_cast<int>(dstH);
            blit.load_op = SDL_GPU_LOADOP_CLEAR;
            {
                auto& bus = NESBus::instance();
                if (bus.useBackdropForBackground && bus.ppu) {
                    uint32_t c = bus.ppu->getBackdropColor();
                    blit.clear_color = {
                        ((c >> 16) & 0xFF) / 255.0f,
                        ((c >> 8) & 0xFF) / 255.0f,
                        (c & 0xFF) / 255.0f,
                        1.0f
                    };
                } else {
                    blit.clear_color = {0, 0, 0, 1};
                }
            }
            blit.filter = SDL_GPU_FILTER_NEAREST;
            SDL_BlitGPUTexture(cmd, &blit);
        } else {
            SDL_GPUColorTargetInfo target = {};
            target.texture = swapchain;
            target.load_op = SDL_GPU_LOADOP_CLEAR;
            target.clear_color = {0, 0, 0, 1};
            SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(cmd, &target, 1, nullptr);
            SDL_EndGPURenderPass(pass);
        }

        uiLayer->render(cmd, swapchain, session);
        SDL_SubmitGPUCommandBuffer(cmd);
        uiLayer->renderPlatformWindows();
    }

    void initMenu(AppSettings &s, IMenuHandler &h, IInputContext &input) override {
        this->input = &input;
        uiLayer->init(&s, &h, &input);
    }
    bool isMenuOpen() const override { return uiLayer->isMenuOpen(); }
    std::string openFileDialog() override {
        return platformAPI.openFileDialog(window);
    }

    bool getScanLine(int &outRaw) const override {
        return platformAPI.getScanLine(window, outRaw);
    }

    double getRefreshHz() const override {
        SDL_DisplayID displayID = SDL_GetDisplayForWindow(window);
        if (displayID == 0)
            return 60.0;

        if (const SDL_DisplayMode *mode = SDL_GetCurrentDisplayMode(displayID);
            mode && mode->refresh_rate_denominator > 0) {
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
            AppKey::SpeedDown,   AppKey::Reset,        AppKey::Open,
            AppKey::NsfTogglePause, AppKey::NsfNextSong, AppKey::NsfPrevSong,
            AppKey::Reload,
            AppKey::Rewind,
            AppKey::SaveState0, AppKey::SaveState1, AppKey::SaveState2,
            AppKey::SaveState3, AppKey::SaveState4, AppKey::SaveState5,
            AppKey::SaveState6, AppKey::SaveState7, AppKey::SaveState8,
            AppKey::SaveState9,
            AppKey::LoadState0, AppKey::LoadState1, AppKey::LoadState2,
            AppKey::LoadState3, AppKey::LoadState4, AppKey::LoadState5,
            AppKey::LoadState6, AppKey::LoadState7, AppKey::LoadState8,
            AppKey::LoadState9,
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
            AppKey::SpeedDown,   AppKey::Reset,        AppKey::Open,
            AppKey::NsfTogglePause, AppKey::NsfNextSong, AppKey::NsfPrevSong,
            AppKey::Reload,
            AppKey::Rewind,
            AppKey::SaveState0, AppKey::SaveState1, AppKey::SaveState2,
            AppKey::SaveState3, AppKey::SaveState4, AppKey::SaveState5,
            AppKey::SaveState6, AppKey::SaveState7, AppKey::SaveState8,
            AppKey::SaveState9,
            AppKey::LoadState0, AppKey::LoadState1, AppKey::LoadState2,
            AppKey::LoadState3, AppKey::LoadState4, AppKey::LoadState5,
            AppKey::LoadState6, AppKey::LoadState7, AppKey::LoadState8,
            AppKey::LoadState9,
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
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            if (ev.window.windowID == SDL_GetWindowID(window)) {
                out = {AppEventType::Quit};
                return true;
            }
            return false;
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
            out = {AppEventType::WindowResized};
            return true;
        case SDL_EVENT_MOUSE_MOTION:
            out = {AppEventType::MouseMoved};
            return true;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            out = {AppEventType::MouseButtonDown};
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
        case SDL_EVENT_DROP_FILE: {
            if (ev.drop.data) {
                out.type = AppEventType::DropFile;
                out.dropPath = ev.drop.data;
                return true;
            }
            return false;
        }
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
    SDL_GPUDevice *gpuDevice = nullptr;
    SDL_GPUTexture *nesTexture = nullptr;
    SDL_GPUTransferBuffer *nesTransfer = nullptr;
    ISDLWindowAPI &platformAPI;
    IInputContext *input = nullptr;

    std::unique_ptr<ImGuiLayer> uiLayer;
};