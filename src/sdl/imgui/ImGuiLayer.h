#pragma once
#include "../../AppSettings.h"
#include "../../IFileSession.h"
#include "../../IInputContext.h"
#include "../../IMenuHandler.h"
#include "../../core/NESBus.h"
#include "../../lang/LanguageRegistry.h"
#include "backends/imgui_impl_sdl3.h"
#include "backends/imgui_impl_sdlgpu3.h"
#include "imgui.h"
#include "PressStart2P-Regular.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>
#include <chrono>
#include <filesystem>
#include <format>
#include <functional>
#include <string>
#include <charconv>
#include <array>

class ImGuiLayer {
public:
    ImGuiLayer(SDL_Window *window, SDL_GPUDevice *gpuDevice)
        : window(window), gpuDevice(gpuDevice), memAddrBuf(4, '0') {

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui::StyleColorsDark();

        ImGui_ImplSDL3_InitForSDLGPU(window);

        ImGui_ImplSDLGPU3_InitInfo initInfo = {};
        initInfo.Device = gpuDevice;
        initInfo.ColorTargetFormat = SDL_GetGPUSwapchainTextureFormat(gpuDevice, window);
        initInfo.MSAASamples = SDL_GPU_SAMPLECOUNT_1;
        initInfo.SwapchainComposition = SDL_GPU_SWAPCHAINCOMPOSITION_SDR;
        initInfo.PresentMode = SDL_GPU_PRESENTMODE_IMMEDIATE;
        ImGui_ImplSDLGPU3_Init(&initInfo);

        auto black = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
        auto grey = ImVec4(74.0f/255.0f, 77.0f/255.0f, 74.0f/255.0f, 1.0f);
        auto lightGrey = ImVec4(106.0f/255.0f, 109.0f/255.0f, 106.0f/255.0f, 1.0f);

        auto darkBlue = ImVec4(0.0f/255.0f, 19.0f/255.0f, 128.0f/255.0f, 1.0f);
        auto blue = ImVec4(24.0f/255.0f, 80.0f/255.0f, 199.0f/255.0f, 1.0f);
        auto lightBlue = ImVec4(104.0f/255.0f, 166.0f/255.0f, 1.0f, 1.0f);

        auto transparent = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);

        ImGuiStyle& style = ImGui::GetStyle();
        float scale = 2.0f;
        float tileDim = 8.0f * scale;
        auto tileSize = ImVec2(tileDim, tileDim);
        auto zeroSize = ImVec2(0.0f, 0.0f);
        auto paddingX = ImVec2(tileDim, 0.0f);

        style.FontScaleMain = scale;
        style.WindowTitleAlign = ImVec2(0.5f, 0.5f);
        style.WindowBorderSize = 0.0f;
        style.FrameBorderSize = 0.0f;
        style.PopupBorderSize = 0.0f;
        style.ChildBorderSize = 0.0f;
        style.FramePadding = zeroSize;
        style.DisplaySafeAreaPadding = zeroSize;
        style.WindowPadding = tileSize;
        style.ItemSpacing = paddingX;
        style.ItemInnerSpacing = tileSize;
        style.CellPadding = paddingX;

        style.FrameRounding = tileDim;

        style.AntiAliasedLines = false;
        style.AntiAliasedLinesUseTex = false;
        style.AntiAliasedFill = false;

        style.Colors[ImGuiCol_Border] = black;

        style.Colors[ImGuiCol_WindowBg] = grey;
        style.Colors[ImGuiCol_ChildBg] = grey;
        style.Colors[ImGuiCol_MenuBarBg] = grey;
        style.Colors[ImGuiCol_PopupBg] = grey;

        style.Colors[ImGuiCol_TitleBg] = black;
        style.Colors[ImGuiCol_TitleBgActive] = darkBlue;

        style.Colors[ImGuiCol_FrameBg] = darkBlue;
        style.Colors[ImGuiCol_FrameBgHovered] = lightBlue;
        style.Colors[ImGuiCol_FrameBgActive] = blue;

        style.Colors[ImGuiCol_Button] = darkBlue;
        style.Colors[ImGuiCol_ButtonHovered] = lightBlue;
        style.Colors[ImGuiCol_ButtonActive] = blue;

        style.Colors[ImGuiCol_Header] = darkBlue;
        style.Colors[ImGuiCol_HeaderHovered] = lightBlue;
        style.Colors[ImGuiCol_HeaderActive] = blue;

        style.Colors[ImGuiCol_SliderGrab] = blue;
        style.Colors[ImGuiCol_SliderGrabActive] = lightBlue;

        style.Colors[ImGuiCol_CheckboxSelectedBg] = blue;
        style.Colors[ImGuiCol_CheckMark] = lightBlue;

        style.Colors[ImGuiCol_TableHeaderBg] = transparent;
        style.Colors[ImGuiCol_TableRowBg] = grey;
        style.Colors[ImGuiCol_TableRowBgAlt] = lightGrey;

        style.Colors[ImGuiCol_TextSelectedBg] = lightBlue;

        ImFontConfig fontConfig;
        fontConfig.FontDataOwnedByAtlas = false;
        fontConfig.GlyphOffset = ImVec2(0.5f, -4.0f);
        fontConfig.ExtraSizeScale = 0.5f;

        ImGuiIO &io = ImGui::GetIO();
        io.Fonts->AddFontFromMemoryTTF((void*)PressStart2P_Regular, 116008, 16.0f, &fontConfig);
        io.IniFilename = nullptr;
        io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    }

    ~ImGuiLayer() {
        ImGui_ImplSDLGPU3_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
    }

    void init(AppSettings *s, IMenuHandler *h, IInputContext *i) {
        settings = s;
        handler = h;
        input = i;
        applyPresentMode();
    }

    bool processEvent(const SDL_Event *ev) {
        ImGui_ImplSDL3_ProcessEvent(ev);

        if (!imguiFocused)
            return false;

        if (waitingForKey) {
            if (ev->type == SDL_EVENT_KEY_DOWN && !ev->key.repeat) {
                SDL_Scancode sc = ev->key.scancode;
                if (!KeyChord::isModifierScancode(sc))
                    assignWaitingKey(sc, ev->key.mod);
                return true;
            }
            if (ev->type == SDL_EVENT_KEY_UP &&
                KeyChord::isModifierScancode(ev->key.scancode)) {
                assignWaitingKey(ev->key.scancode, SDL_KMOD_NONE);
                return true;
            }
        }

        if (ev->type == SDL_EVENT_MOUSE_BUTTON_DOWN ||
            ev->type == SDL_EVENT_MOUSE_BUTTON_UP ||
            ev->type == SDL_EVENT_MOUSE_WHEEL ||
            ev->type == SDL_EVENT_KEY_DOWN ||
            ev->type == SDL_EVENT_KEY_UP ||
            ev->type == SDL_EVENT_TEXT_INPUT) {
            return true;
        }

        return false;
    }

    bool isMenuOpen() const { return menuOpen; }

    void render(SDL_GPUCommandBuffer *cmd, SDL_GPUTexture *swapchain, IFileSession *session) {
        if (input)
            input->setInputBlocked(imguiFocused);

        NESCoreBase *core = session ? &session->getCore() : nullptr;

        ImGui_ImplSDLGPU3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        menuOpen = false;

        ImGui::PushStyleVarY(ImGuiStyleVar_WindowPadding, 0);

        if (handler->isMenuVisible() && ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu(tr("file"))) {
                menuOpen = true;
                if (ImGui::MenuItem(tr("file.open")))
                    handler->onOpen();
                if (session && ImGui::MenuItem(tr("file.reload")))
                    handler->onReload();
                if (session && ImGui::MenuItem(tr("file.close")))
                    handler->onClose();
                if (session) {
                    if (ImGui::BeginMenu(tr("file.saveState"))) {
                        menuOpen = true;
                        for (int i = 1; i <= 9; i++)
                            drawStateSlot(tr, session, handler, i, true);
                        drawStateSlot(tr, session, handler, 0, true);
                        ImGui::EndMenu();
                    }
                    if (ImGui::BeginMenu(tr("file.loadState"))) {
                        menuOpen = true;
                        for (int i = 1; i <= 9; i++)
                            drawStateSlot(tr, session, handler, i, false);
                        drawStateSlot(tr, session, handler, 0, false);
                        ImGui::EndMenu();
                    }
                }
                if (ImGui::MenuItem(tr("file.quit")))
                    handler->onQuit();
                ImGui::EndMenu();
            }

            if (settings && core) {
                if (ImGui::BeginMenu(tr("emulation"))) {
                    menuOpen = true;
                    ImGui::MenuItem(tr("emulation.pause"), nullptr, &core->paused);
                    if (ImGui::MenuItem(tr("emulation.reset")))
                        handler->onReset();

                    if (ImGui::BeginMenu(tr("emulation.system"))) {
                        menuOpen = true;
                        static const NESStandard systems[] = {
                            NESStandard::NTSC, NESStandard::PAL, NESStandard::DENDY,
                        };
                        static const char *names[] = { "NTSC", "PAL", "Dendy" };
                        for (int i = 0; i < 3; i++) {
                            if (ImGui::RadioButton(names[i], core->system == systems[i])) {
                                settings->system = i;
                                core->setSystem(systems[i]);
                                handler->onReset();
                            }
                        }
                        ImGui::EndMenu();
                    }

                    if (ImGui::BeginMenu(tr("tools"))) {
                        menuOpen = true;
                        if (ImGui::MenuItem(tr("tools.about_file")))
                            aboutFileOpen = true;
                        if (ImGui::MenuItem(tr("tools.memory_viewer")))
                            memoryViewerOpen = true;
                        ImGui::EndMenu();
                    }

                    ImGui::EndMenu();
                }
            }

            if (settings && ImGui::BeginMenu(tr("settings"))) {
                menuOpen = true;
                if (ImGui::BeginMenu(tr("settings.language"))) {
                    for (auto &reg = LanguageRegistry::instance();
                         auto &[code, lang] : reg.languages()) {
                        if (ImGui::RadioButton(lang->getName(),
                                               reg.getCurrentCode() == code))
                            reg.setCode(code);
                    }
                    ImGui::EndMenu();
                }

                if (ImGui::MenuItem(tr("settings.sync")))
                    syncSettingsOpen = true;

                if (ImGui::MenuItem(tr("settings.graphics")))
                    graphicsSettingsOpen = true;

                if (ImGui::MenuItem(tr("settings.audio")))
                    audioSettingsOpen = true;

                if (ImGui::MenuItem(tr("settings.controls")))
                    controlsOpen = true;

                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
        }

        ImGui::PopStyleVar();

        renderSyncWindow(session);
        renderAudioWindow();
        renderControlsWindow();
        renderGraphicsWindow(session);
        renderAboutFileWindow(session);
        renderMemoryViewerWindow(session);

        imguiFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow) ||
                       ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel) ||
                       waitingForKey;

        ImGui::Render();
        ImGui_ImplSDLGPU3_PrepareDrawData(ImGui::GetDrawData(), cmd);

        if (swapchain) {
            SDL_GPUColorTargetInfo target = {};
            target.texture = swapchain;
            target.load_op = SDL_GPU_LOADOP_LOAD;
            target.store_op = SDL_GPU_STOREOP_STORE;
            SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(cmd, &target, 1, nullptr);
            ImGui_ImplSDLGPU3_RenderDrawData(ImGui::GetDrawData(), cmd, pass);
            SDL_EndGPURenderPass(pass);
        }
    }

    static void renderPlatformWindows() {
        if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }
    }

private:
    void applyPresentMode() {
        if (!settings)
            return;
        if (settings->syncMode == 2)
            settings->vsync = false;
        SDL_SetGPUSwapchainParameters(
            gpuDevice, window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR,
            settings->vsync ? SDL_GPU_PRESENTMODE_VSYNC
                            : SDL_GPU_PRESENTMODE_IMMEDIATE);
    }

    struct Binding {
        const char *label;
        SDL_Scancode *controllerKey;
        AppKey appKey;
    };

    static std::string scancodeName(SDL_Scancode scancode) {
        if (scancode == SDL_SCANCODE_UNKNOWN)
            return "-";
        const char *name = SDL_GetScancodeName(scancode);
        return (name && name[0]) ? std::string(name) : std::string("?");
    }

    Binding bindingAt(int index) {
        if (!input)
            return {"", nullptr, AppKey::Unknown};
        if (index < 10)
            return controllerBinding(input->controllerSettings(0), index);
        if (index < 20)
            return controllerBinding(input->controllerSettings(1), index - 10);
        if (index < 29)
            return appBinding(index - 20);
        return saveLoadBinding(index - 30);
    }

    static Binding controllerBinding(ControllerSettings &s, int index) {
        switch (index) {
        case 0: return {"A", &s.key_A, AppKey::Unknown};
        case 1: return {"B", &s.key_B, AppKey::Unknown};
        case 2: return {"Turbo A", &s.key_TurboA, AppKey::Unknown};
        case 3: return {"Turbo B", &s.key_TurboB, AppKey::Unknown};
        case 4: return {"Select", &s.key_Select, AppKey::Unknown};
        case 5: return {"Start", &s.key_Start, AppKey::Unknown};
        case 6: return {"controls.up", &s.key_Up, AppKey::Unknown};
        case 7: return {"controls.down", &s.key_Down, AppKey::Unknown};
        case 8: return {"controls.left", &s.key_Left, AppKey::Unknown};
        case 9: return {"controls.right", &s.key_Right, AppKey::Unknown};
        default: return {"", nullptr, AppKey::Unknown};
        }
    }

    static Binding appBinding(int index) {
        switch (index) {
        case 0: return {"controls.pause", nullptr, AppKey::Pause};
        case 1: return {"controls.fullscreen", nullptr, AppKey::FullScreen};
        case 2: return {"controls.speedup", nullptr, AppKey::SpeedUp};
        case 3: return {"controls.speeddown", nullptr, AppKey::SpeedDown};
        case 4: return {"controls.reset", nullptr, AppKey::Reset};
        case 5: return {"controls.open", nullptr, AppKey::Open};
        case 6: return {"controls.reload", nullptr, AppKey::Reload};
        case 7: return {"controls.nsf.pause", nullptr, AppKey::NsfTogglePause};
        case 8: return {"controls.nsf.next", nullptr, AppKey::NsfNextSong};
        case 9: return {"controls.nsf.prev", nullptr, AppKey::NsfPrevSong};
        default: return {"", nullptr, AppKey::Unknown};
        }
    }

    static Binding saveLoadBinding(int index) {
        switch (index) {
        case 0: return {"Slot 1", nullptr, AppKey::SaveState0};
        case 1: return {"Slot 2", nullptr, AppKey::SaveState1};
        case 2: return {"Slot 3", nullptr, AppKey::SaveState2};
        case 3: return {"Slot 4", nullptr, AppKey::SaveState3};
        case 4: return {"Slot 5", nullptr, AppKey::SaveState4};
        case 5: return {"Slot 6", nullptr, AppKey::SaveState5};
        case 6: return {"Slot 7", nullptr, AppKey::SaveState6};
        case 7: return {"Slot 8", nullptr, AppKey::SaveState7};
        case 8: return {"Slot 9", nullptr, AppKey::SaveState8};
        case 9: return {"Slot 0", nullptr, AppKey::SaveState9};
        case 10: return {"Slot 1", nullptr, AppKey::LoadState0};
        case 11: return {"Slot 2", nullptr, AppKey::LoadState1};
        case 12: return {"Slot 3", nullptr, AppKey::LoadState2};
        case 13: return {"Slot 4", nullptr, AppKey::LoadState3};
        case 14: return {"Slot 5", nullptr, AppKey::LoadState4};
        case 15: return {"Slot 6", nullptr, AppKey::LoadState5};
        case 16: return {"Slot 7", nullptr, AppKey::LoadState6};
        case 17: return {"Slot 8", nullptr, AppKey::LoadState7};
        case 18: return {"Slot 9", nullptr, AppKey::LoadState8};
        case 19: return {"Slot 0", nullptr, AppKey::LoadState9};
        default: return {"", nullptr, AppKey::Unknown};
        }
    }

    std::string bindingName(const Binding &binding) const {
        if (binding.controllerKey)
            return scancodeName(*binding.controllerKey);
        if (!input)
            return "-";
        return input->appKeyBinding(binding.appKey).name();
    }

    void clearBinding(const Binding &binding) {
        if (binding.controllerKey)
            *binding.controllerKey = SDL_SCANCODE_UNKNOWN;
        else if (input)
            input->setAppKeyBinding(binding.appKey, KeyChord{});
    }

    void assignBinding(const Binding &binding, SDL_Scancode scancode,
                       SDL_Keymod mods) {
        if (binding.controllerKey) {
            *binding.controllerKey = scancode;
            return;
        }
        if (!input)
            return;
        KeyChord c;
        c.scancode = scancode;
        c.mods = KeyChord::modsFromSDL(mods) &
                 ~KeyChord::modBitForScancode(scancode);
        input->setAppKeyBinding(binding.appKey, c);
    }

    void beginSingleBind(int index) {
        waitingForKey = true;
        bindAll = false;
        bindingIndex = index;
    }

    void beginSectionBind(int begin, int count) {
        waitingForKey = true;
        bindAll = true;
        bindingIndex = begin;
        bindSectionBegin = begin;
        bindSectionEnd = begin + count;
    }

    void cancelBinding() {
        waitingForKey = false;
        bindAll = false;
    }

    void assignWaitingKey(SDL_Scancode scancode, SDL_Keymod mods) {
        Binding binding = bindingAt(bindingIndex);
        if (scancode == SDL_SCANCODE_ESCAPE)
            clearBinding(binding);
        else
            assignBinding(binding, scancode, mods);
        if (bindAll) {
            bindingIndex++;
            if (bindingIndex >= bindSectionEnd) {
                waitingForKey = false;
                bindAll = false;
            }
        } else {
            waitingForKey = false;
        }
    }

    static void spacing() {
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImGui::GetStyle().ItemInnerSpacing);
        ImGui::Spacing();
        ImGui::PopStyleVar();
    }

    static bool button(const char* text) {
        ImGui::PushStyleVarX(ImGuiStyleVar_FramePadding, ImGui::GetStyle().ItemInnerSpacing.x);
        bool res = ImGui::Button(text);
        ImGui::PopStyleVar();
        return res;
    }

    static int inputTextResizeCallback(ImGuiInputTextCallbackData *data) {
        if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
            auto *str = static_cast<std::string *>(data->UserData);
            str->resize(data->BufTextLen);
            data->Buf = str->data();
            data->BufSize = static_cast<int>(str->capacity() + 1);
        }
        return 0;
    }

    static bool inputText(const char *label, std::string &str, ImGuiInputTextFlags flags = 0) {
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
        flags |= ImGuiInputTextFlags_CallbackResize;
        bool res = ImGui::InputText(label, str.data(), str.capacity() + 1, flags,
                                    inputTextResizeCallback, &str);
        ImGui::PopStyleVar();
        return res;
    }

    void renderBindingRow(int index) {
        Binding binding = bindingAt(index);
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(tr(binding.label));
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(bindingName(binding).c_str());
        ImGui::TableNextColumn();
        bool isThisRowWaiting = waitingForKey && bindingIndex == index;
        std::string buttonText = isThisRowWaiting ? tr("controls.cancel")
                                              : tr("controls.set");
        buttonText += "##bind" + std::to_string(index);
        if (!waitingForKey || isThisRowWaiting) {
            if (button(buttonText.c_str())) {
                if (isThisRowWaiting)
                    cancelBinding();
                else
                    beginSingleBind(index);
            }
        }
        ImGui::SameLine();
        std::string clearBtn =
            std::string(tr("controls.clear")) + "##clear" + std::to_string(index);
        if (!waitingForKey && button(clearBtn.c_str()))
            clearBinding(binding);
    }

    void renderBindingsSection(const char *nameId, int begin, int count) {
        if (!ImGui::CollapsingHeader(tr(nameId))) {
            spacing();
            return;
        }

        spacing();

        bool isThisSectionBinding =
            waitingForKey && bindAll && bindSectionBegin == begin;
        std::string btnLabel = isThisSectionBinding ? tr("controls.cancel")
                                                    : tr("controls.bind_all");
        btnLabel += "##bindall" + std::string(nameId);
        if (!waitingForKey || isThisSectionBinding) {
            if (button(btnLabel.c_str())) {
                if (isThisSectionBinding)
                    cancelBinding();
                else
                    beginSectionBind(begin, count);
            }
        }
        ImGui::SameLine();
        std::string clearAllBtn =
            std::string(tr("controls.clear_all")) + "##clearall" + nameId;
        if (!waitingForKey && button(clearAllBtn.c_str())) {
            for (int i = 0; i < count; i++)
                clearBinding(bindingAt(begin + i));
        }

        spacing();

        if (ImGui::BeginTable(nameId, 3, TABLE_FLAGS)) {
            ImGui::TableSetupColumn(tr("controls.action"));
            ImGui::TableSetupColumn(tr("controls.key"));
            ImGui::TableSetupColumn("");
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(tr("controls.action"));
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(tr("controls.key"));
            ImGui::TableNextColumn();
            for (int i = 0; i < count; i++)
                renderBindingRow(begin + i);
            ImGui::EndTable();
        }

        spacing();
    }

    void renderSyncWindow(IFileSession *session) {
        if (!syncSettingsOpen || !settings)
            return;

        menuOpen = true;
        if (!ImGui::Begin(tr("settings.sync"), &syncSettingsOpen, WINDOW_FLAGS)) {
            ImGui::End();
            return;
        }

        ImGui::SliderFloat(tr("settings.speed"), &settings->speed, 0.1f, 8.0f, "%.1f");
        ImGui::SliderFloat(tr("settings.speed1"), &settings->speed1, 0.1f, 8.0f, "%.1f");
        ImGui::SliderFloat(tr("settings.speed2"), &settings->speed2, 0.1f, 8.0f, "%.1f");
        spacing();

        ImGui::TextUnformatted(tr("settings.sync_mode"));
        ImGui::RadioButton(tr("settings.sync.none"), &settings->syncMode, 0);
        addTooltipToLastItem(tr("settings.sync.none.tooltip"));
        ImGui::RadioButton(tr("settings.sync.refresh_rate"), &settings->syncMode, 1);
        addTooltipToLastItem(tr("settings.sync.refresh_rate.tooltip"));
        if (ImGui::RadioButton(tr("settings.sync.scanline"), &settings->syncMode, 2))
            applyPresentMode();
        addTooltipToLastItem(tr("settings.sync.scanline.tooltip"));

        if (settings->syncMode == 2) {
            ImGui::SliderInt(tr("settings.scanline.buffer"), &settings->scanlineBufferMs, 1, 20);
            addTooltipToLastItem(tr("settings.scanline.buffer.tooltip"));
        }

        if (session) {
            double speed = session->getCore().speed;
            ImGui::TextUnformatted(std::format("{}: {:.2f}%", tr("settings.target_speed"), speed * 100.0).c_str());
        }

        if (settings->syncMode != 2) {
            spacing();
            if (ImGui::Checkbox(tr("settings.vsync"), &settings->vsync))
                applyPresentMode();
        }

        ImGui::End();
    }

    void renderAudioWindow() {
        if (!audioSettingsOpen || !settings)
            return;

        menuOpen = true;
        if (!ImGui::Begin(tr("settings.audio"), &audioSettingsOpen, WINDOW_FLAGS)) {
            ImGui::End();
            return;
        }

        AudioSettings &audioSettings = settings->audioSettings;
        ImGui::SliderFloat(tr("settings.volume"), &audioSettings.volume, 0.0f, 2.5f, "%.1f");

        spacing();

        ImGui::Checkbox(tr("settings.audio.adjust_pitch"), &settings->adjustPitch);
        ImGui::Checkbox(tr("settings.audio.use_filters"), &audioSettings.useFilters);

        ImGui::End();
    }

    void renderControlsWindow() {
        if (!controlsOpen || !input)
            return;
        menuOpen = true;
        bool wasOpen = controlsOpen;
        if (!ImGui::Begin(tr("controls.title"), &controlsOpen, WINDOW_FLAGS)) {
            ImGui::End();
            if (wasOpen && !controlsOpen && waitingForKey)
                cancelBinding();
            return;
        }
        if (wasOpen && !controlsOpen && waitingForKey)
            cancelBinding();

        renderBindingsSection("controls.pad1", 0, 10);
        renderBindingsSection("controls.pad2", 10, 10);
        renderBindingsSection("controls.emulation", 20, 7);
        renderBindingsSection("controls.nsf", 27, 3);
        renderBindingsSection("controls.savestate", 30, 20);

        ImGui::End();
    }

    void renderAboutFileWindow(IFileSession *session) {
        if (!aboutFileOpen || !session)
            return;
        menuOpen = true;
        if (!ImGui::Begin(tr("tools.about_file"), &aboutFileOpen, WINDOW_FLAGS)) {
            ImGui::End();
            return;
        }
        std::string info = session->getInfo();
        ImGui::TextUnformatted(info.c_str());
        ImGui::End();
    }

    void renderGraphicsWindow(IFileSession *session) {
        if (!graphicsSettingsOpen || !settings)
            return;

        menuOpen = true;
        if (!ImGui::Begin(tr("settings.graphics"), &graphicsSettingsOpen, WINDOW_FLAGS)) {
            ImGui::End();
            return;
        }

        if (ImGui::Checkbox(tr("settings.graphics.use_backdrop"), &settings->useBackdropForBackground))
            NESBus::instance().useBackdropForBackground = settings->useBackdropForBackground;

        if (ImGui::Checkbox(tr("settings.graphics.preserve_aspect"), &settings->preserveAspectRatio))
            NESBus::instance().preserveAspectRatio = settings->preserveAspectRatio;

        ImGui::End();
    }

    void renderMemoryViewerWindow(IFileSession *session) {
        if (!memoryViewerOpen || !session)
            return;
        menuOpen = true;
        if (!ImGui::Begin(tr("tools.memory_viewer"), &memoryViewerOpen, WINDOW_FLAGS)) {
            ImGui::End();
            return;
        }

        NESCoreBase &core = session->getCore();

        ImGui::SetNextItemWidth(ImGui::CalcTextSize("FFFF ").x);
        if (inputText(tr("tools.memory_viewer.address"), memAddrBuf, HEX_INPUT_FLAGS)) {
            if (uint32_t v = 0;
                std::from_chars(memAddrBuf.data(), memAddrBuf.data() + memAddrBuf.size(), v, 16).ec == std::errc())
                setMemAddr(static_cast<uint16_t>(v & 0xFFFF));
        }

        if (ImGui::IsWindowHovered() && ImGui::GetIO().MouseWheel != 0.0f) {
            int step = -static_cast<int>(ImGui::GetIO().MouseWheel) * MEM_COLS;
            setMemAddr(static_cast<uint16_t>((memBase + step) & 0xFFFF));
        }

        spacing();

        for (int i = 0; i < MEM_ROWS * MEM_COLS; i++)
            memCache[i] = core.peekMemory(static_cast<uint16_t>(memBase + i));

        ImGui::PushStyleVarX(ImGuiStyleVar_CellPadding, 0);

        if (ImGui::BeginTable("memview", MEM_COLS + 1, TABLE_FLAGS)) {
            ImGui::TableSetupColumn("");
            for (int c = 0; c < MEM_COLS; c++)
                ImGui::TableSetupColumn(std::format("{:X}", c).c_str());

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            for (int c = 0; c < MEM_COLS; c++) {
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(std::format("{:X}", c).c_str());
            }
            for (int r = 0; r < MEM_ROWS; r++) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(std::format("{:04X} ", (memBase + r * MEM_COLS) & 0xFFFF).c_str());
                for (int c = 0; c < MEM_COLS; c++) {
                    ImGui::TableNextColumn();
                    int idx = r * MEM_COLS + c;
                    auto addr = static_cast<uint16_t>((memBase + idx) & 0xFFFF);

                    memEditBuf[idx] = std::format("{:02X}", memCache[idx]);
                    ImGui::SetNextItemWidth(ImGui::CalcTextSize("FF ").x);

                    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));

                    if (inputText(std::format("##mem{}", idx).c_str(), memEditBuf[idx], HEX_INPUT_FLAGS)) {
                        if (uint32_t v = 0;
                            std::from_chars(memEditBuf[idx].data(),
                                            memEditBuf[idx].data() + memEditBuf[idx].size(), v, 16).ec == std::errc())
                            core.writeMemory(addr, static_cast<uint8_t>(v & 0xFF));
                    }

                    ImGui::PopStyleColor();
                }
            }
            ImGui::EndTable();
        }

        ImGui::PopStyleVar();

        ImGui::End();
    }

    void setMemAddr(uint16_t addr) {
        memBase = addr;
        memAddrBuf = std::format("{:04X}", memBase);
    }

    static void addTooltipToLastItem(const char *text) {
        if (!ImGui::IsItemHovered()) return;
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetIO().DisplaySize.x);
        ImGui::TextUnformatted(text);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }

    static void drawStateSlot(const std::function<const char*(const char*)>& tr,
                              IFileSession* session, IMenuHandler* mh,
                              int slot, bool isSave) {
        std::string label = "Slot " + std::to_string(slot);
        auto savePath = std::filesystem::path(session->path).parent_path() / "saves"
                        / (session->filename + "." + std::to_string(slot) + ".sav");
        if (std::filesystem::exists(savePath)) {
            auto ftime = std::filesystem::last_write_time(savePath);
            auto sctp = std::chrono::clock_cast<std::chrono::system_clock>(ftime);
            auto tt = std::chrono::system_clock::to_time_t(sctp);
            std::tm tm;
            localtime_s(&tm, &tt);
            char buf[32];
            std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", &tm);
            label += "  (" + std::string(buf) + ")";
        }
        if (ImGui::MenuItem(label.c_str()))
            isSave ? mh->onSaveState(slot) : mh->onLoadState(slot);
    }

    static constexpr ImGuiWindowFlags WINDOW_FLAGS = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar;
    static constexpr ImGuiTableFlags TABLE_FLAGS = ImGuiTableFlags_RowBg;
    static constexpr ImGuiInputTextFlags HEX_INPUT_FLAGS =
        ImGuiInputTextFlags_CharsHexadecimal |
        ImGuiInputTextFlags_CharsUppercase |
        ImGuiInputTextFlags_CharsNoBlank |
        ImGuiInputTextFlags_EnterReturnsTrue |
        ImGuiInputTextFlags_AutoSelectAll;

    SDL_Window *window = nullptr;
    SDL_GPUDevice *gpuDevice = nullptr;
    AppSettings *settings = nullptr;
    bool menuOpen = false;
    IMenuHandler *handler = nullptr;
    IInputContext *input = nullptr;

    bool controlsOpen = false;
    bool syncSettingsOpen = false;
    bool audioSettingsOpen = false;
    bool graphicsSettingsOpen = false;
    bool aboutFileOpen = false;
    bool memoryViewerOpen = false;

    static constexpr int MEM_ROWS = 16;
    static constexpr int MEM_COLS = 16;
    uint16_t memBase = 0x0000;
    std::string memAddrBuf;
    uint8_t memCache[MEM_ROWS * MEM_COLS] = {};
    std::array<std::string, MEM_ROWS * MEM_COLS> memEditBuf;

    bool bindAll = false;
    int bindingIndex = 0;
    int bindSectionBegin = 0;
    int bindSectionEnd = 0;

    bool waitingForKey = false;

    bool imguiFocused = false;
};
