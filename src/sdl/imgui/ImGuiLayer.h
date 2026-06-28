#pragma once
#include "../../AppSettings.h"
#include "../../IFileSession.h"
#include "../../IInputContext.h"
#include "../../IMenuHandler.h"
#include "../../lang/LanguageRegistry.h"
#include "backends/imgui_impl_sdl3.h"
#include "backends/imgui_impl_sdlrenderer3.h"
#include "imgui.h"
#include "PressStart2P-Regular.h"
#include <SDL3/SDL.h>
#include <format>
#include <string>

class ImGuiLayer {
public:
    ImGuiLayer(SDL_Window *window, SDL_Renderer *renderer) {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui::StyleColorsDark();

        ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
        ImGui_ImplSDLRenderer3_Init(renderer);

        ImVec4 black = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
        ImVec4 grey = ImVec4(74.0f/255.0f, 77.0f/255.0f, 74.0f/255.0f, 1.0f);
        ImVec4 lightGrey = ImVec4(106.0f/255.0f, 109.0f/255.0f, 106.0f/255.0f, 1.0f);

        ImVec4 darkTeal = ImVec4(0.0f, 46.0f/255.0f, 85.0f/255.0f, 1.0f);
        ImVec4 teal = ImVec4(0.0f, 110.0f/255.0f, 138.0f/255.0f, 1.0f);
        ImVec4 lightTeal = ImVec4(71.0f/255.0f, 193.0f/255.0f, 197.0f/255.0f, 1.0f);

        ImVec4 darkBlue = ImVec4(0.0f/255.0f, 19.0f/255.0f, 128.0f/255.0f, 1.0f);
        ImVec4 blue = ImVec4(24.0f/255.0f, 80.0f/255.0f, 199.0f/255.0f, 1.0f);
        ImVec4 lightBlue = ImVec4(104.0f/255.0f, 166.0f/255.0f, 1.0f, 1.0f);

        ImVec4 transparent = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);

        ImGuiStyle& style = ImGui::GetStyle();
        float scale = 2.0f;
        float tileDim = 8.0f * scale;
        auto tileSize = ImVec2(tileDim, tileDim);
        auto tileSizeHalf = ImVec2(tileDim * 0.5f, tileDim * 0.5f);
        auto zeroSize = ImVec2(0.0f, 0.0f);

        style.WindowTitleAlign = ImVec2(0.5f, 0.5f);
        style.WindowBorderSize = 0.0f;
        style.FrameBorderSize = 0.0f;
        style.PopupBorderSize = 0.0f;
        style.ChildBorderSize = 0.0f;
        style.FramePadding = zeroSize;
        style.DisplaySafeAreaPadding = zeroSize;
        style.WindowPadding = tileSize;
        style.ItemSpacing = tileSize;
        style.ItemInnerSpacing = tileSize;
        style.CellPadding = tileSizeHalf;

        style.AntiAliasedLines = false;
        style.AntiAliasedLinesUseTex = false;
        style.AntiAliasedFill = false;

        style.Colors[ImGuiCol_Border] = black;

        style.Colors[ImGuiCol_WindowBg] = grey;
        style.Colors[ImGuiCol_ChildBg] = grey;
        style.Colors[ImGuiCol_MenuBarBg] = grey;
        style.Colors[ImGuiCol_PopupBg] = grey;
        style.Colors[ImGuiCol_TitleBgActive] = grey;

        style.Colors[ImGuiCol_TextDisabled] = lightGrey;

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

        ImFontConfig fontConfig;
        fontConfig.FontDataOwnedByAtlas = false;
        fontConfig.GlyphOffset.x = 1.0f;

        ImGuiIO &io = ImGui::GetIO();
        io.Fonts->AddFontFromMemoryTTF((void*)PressStart2P_Regular, 116008, 8.0f, &fontConfig);
        io.Fonts->Build();
        io.IniFilename = nullptr;
        io.FontGlobalScale = scale;
    }

    ~ImGuiLayer() {
        ImGui_ImplSDLRenderer3_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
    }

    void init(AppSettings *s, IMenuHandler *h, IInputContext *i) {
        settings = s;
        handler = h;
        input = i;
    }

    bool processEvent(const SDL_Event *ev) {
        ImGui_ImplSDL3_ProcessEvent(ev);
        if (!waitingForKey)
            return false;
        if (ev->type == SDL_EVENT_KEY_DOWN && !ev->key.repeat) {
            SDL_Scancode sc = ev->key.scancode;
            if (KeyChord::isModifierScancode(sc))
                return true;
            assignWaitingKey(sc, ev->key.mod);
            return true;
        }
        if (ev->type == SDL_EVENT_KEY_UP) {
            SDL_Scancode sc = ev->key.scancode;
            if (KeyChord::isModifierScancode(sc)) {
                assignWaitingKey(sc, SDL_KMOD_NONE);
                return true;
            }
        }
        return false;
    }

    bool isMenuOpen() const { return menuOpen; }

    void render(SDL_Renderer *renderer, IFileSession *session, double baseSpeed) {
        if (input)
            input->setInputBlocked(controlsOpen);

        NESCoreBase *core = session ? &session->getCore() : nullptr;

        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        menuOpen = false;

        if (handler->isMenuVisible() && ImGui::BeginMainMenuBar()) {
            ImGui::SetCursorPosX(0.0f);

            if (ImGui::BeginMenu(tr("file"))) {
                menuOpen = true;
                if (ImGui::MenuItem(tr("file.open")))
                    handler->onOpen();
                if (session && ImGui::MenuItem(tr("file.reload")))
                    handler->onReload();
                if (session && ImGui::MenuItem(tr("file.close")))
                    handler->onClose();
                if (ImGui::MenuItem(tr("file.quit")))
                    handler->onQuit();
                ImGui::EndMenu();
            }

            if (settings && core) {
                if (ImGui::BeginMenu(tr("emulation"))) {
                    menuOpen = true;
                    // TODO: NES, PAL, DENDY
                    /*if (ImGui::BeginMenu(tr("emulation.system"))) {
                        int sub = core->pal ? 1 : 0;
                        if (ImGui::RadioButton("NTSC", &sub, 0))
                            core->pal = false;
                        if (ImGui::RadioButton("PAL", &sub, 1))
                            core->pal = true;
                        ImGui::EndMenu();
                    }*/
                    ImGui::MenuItem(tr("emulation.pause"), nullptr, &core->paused);
                    if (ImGui::MenuItem(tr("emulation.reset")))
                        handler->onReset();

                    ImGui::EndMenu();
                }
            }

            if (settings && ImGui::BeginMenu(tr("settings"))) {
                menuOpen = true;
                if (ImGui::BeginMenu(tr("settings.language"))) {
                    auto &reg = LanguageRegistry::instance();
                    for (auto &[code, lang] : reg.languages()) {
                        if (ImGui::RadioButton(lang->getName(),
                                               reg.getCurrentCode() == code))
                            reg.setCode(code);
                    }
                    ImGui::EndMenu();
                }

                if (ImGui::MenuItem(tr("settings.sync")))
                    syncSettingsOpen = true;

                if (ImGui::MenuItem(tr("settings.audio")))
                    audioSettingsOpen = true;

                if (ImGui::MenuItem(tr("settings.controls")))
                    controlsOpen = true;

                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
        }

        renderSyncWindow(renderer, session, baseSpeed);
        renderAudioWindow();
        renderControlsWindow();

        //ImGui::ShowStyleEditor();

        ImGui::Render();
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
    }

private:
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
        return appBinding(index - 20);
    }

    Binding controllerBinding(ControllerSettings &s, int index) {
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

    Binding appBinding(int index) {
        switch (index) {
        case 0: return {"controls.pause", nullptr, AppKey::Pause};
        case 1: return {"controls.fullscreen", nullptr, AppKey::FullScreen};
        case 2: return {"controls.speedup", nullptr, AppKey::SpeedUp};
        case 3: return {"controls.speeddown", nullptr, AppKey::SpeedDown};
        case 4: return {"controls.reset", nullptr, AppKey::Reset};
        case 5: return {"controls.nsf.pause", nullptr, AppKey::NsfTogglePause};
        case 6: return {"controls.nsf.next", nullptr, AppKey::NsfNextSong};
        case 7: return {"controls.nsf.prev", nullptr, AppKey::NsfPrevSong};
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

    void renderBindingRow(int index) {
        Binding binding = bindingAt(index);
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(tr(binding.label));
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(bindingName(binding).c_str());
        ImGui::TableNextColumn();
        bool isThisRowWaiting = waitingForKey && bindingIndex == index;
        std::string button = isThisRowWaiting ? tr("controls.cancel")
                                              : tr("controls.set");
        button += "##bind" + std::to_string(index);
        ImGui::BeginDisabled(waitingForKey && !isThisRowWaiting);
        if (ImGui::Button(button.c_str())) {
            if (isThisRowWaiting)
                cancelBinding();
            else
                beginSingleBind(index);
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        std::string clearBtn =
            std::string(tr("controls.clear")) + "##clear" + std::to_string(index);
        ImGui::BeginDisabled(waitingForKey);
        if (ImGui::Button(clearBtn.c_str()))
            clearBinding(binding);
        ImGui::EndDisabled();
    }

    void renderBindingsSection(const char *nameId, int begin, int count) {
        if (!ImGui::CollapsingHeader(tr(nameId)))
            return;

        bool isThisSectionBinding =
            waitingForKey && bindAll && bindSectionBegin == begin;
        std::string btnLabel = isThisSectionBinding ? tr("controls.cancel")
                                                    : tr("controls.bind_all");
        btnLabel += "##bindall" + std::string(nameId);
        ImGui::BeginDisabled(waitingForKey && !isThisSectionBinding);
        if (ImGui::Button(btnLabel.c_str())) {
            if (isThisSectionBinding)
                cancelBinding();
            else
                beginSectionBind(begin, count);
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        std::string clearAllBtn =
            std::string(tr("controls.clear_all")) + "##clearall" + nameId;
        ImGui::BeginDisabled(waitingForKey);
        if (ImGui::Button(clearAllBtn.c_str())) {
            for (int i = 0; i < count; i++)
                clearBinding(bindingAt(begin + i));
        }
        ImGui::EndDisabled();

        if (ImGui::BeginTable(nameId, 3, ImGuiTableFlags_None)) {
            ImGui::TableSetupColumn(tr("controls.action"));
            ImGui::TableSetupColumn(tr("controls.key"));
            ImGui::TableSetupColumn("");
            ImGui::TableHeadersRow();
            for (int i = 0; i < count; i++)
                renderBindingRow(begin + i);
            ImGui::EndTable();
        }

        ImGui::TextUnformatted("");
    }

    void renderSyncWindow(SDL_Renderer *renderer, IFileSession *session, double baseSpeed) {
        if (!syncSettingsOpen || !settings)
            return;

        menuOpen = true;
        if (!ImGui::Begin(tr("settings.sync"), &syncSettingsOpen, WINDOW_FLAGS)) {
            ImGui::End();
            return;
        }

        ImGui::BeginDisabled(settings->syncMode == 2);
        if (ImGui::Checkbox(tr("settings.vsync"), &settings->vsync)) {
            SDL_SetRenderVSync(renderer, settings->vsync ? 1 : 0);
        }
        ImGui::EndDisabled();

        ImGui::TextUnformatted("");

        ImGui::TextUnformatted(tr("settings.sync_mode"));

        ImGui::RadioButton(tr("settings.sync.none"), &settings->syncMode, 0);
        addTooltipToLastItem(tr("settings.sync.none.tooltip"));

        ImGui::RadioButton(tr("settings.sync.refresh_rate"), &settings->syncMode, 1);
        addTooltipToLastItem(tr("settings.sync.refresh_rate.tooltip"));

        if (ImGui::RadioButton(tr("settings.sync.scanline"), &settings->syncMode, 2)) {
            settings->vsync = false;
            if (renderer) {
                SDL_SetRenderVSync(renderer, 0);
            }
        }
        addTooltipToLastItem(tr("settings.sync.scanline.tooltip"));

        ImGui::BeginDisabled(settings->syncMode != 2);
        ImGui::SliderInt(tr("settings.scanline.buffer"), &settings->scanlineBufferMs, 0, 20);
        addTooltipToLastItem(tr("settings.scanline.buffer.tooltip"));
        ImGui::EndDisabled();

        if (session) {
            ImGui::TextUnformatted("");

            double speed = session->getCore().speed;
            ImGui::TextUnformatted(
                std::format("{}: {:.2f}%", tr("settings.current_speed"),
                            speed * 100.0).c_str());
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
        ImGui::Checkbox(tr("settings.audio.reduce_clicks"), &audioSettings.reduceClicks);
        ImGui::Checkbox(tr("settings.audio.adjust_pitch"), &settings->adjustPitch);

        ImGui::TextUnformatted("");

        if (ImGui::CollapsingHeader(tr("settings.audio.filters"))) {
            ImGui::Checkbox(tr("settings.audio.hp90"), &audioSettings.useFilter90);
            ImGui::Checkbox(tr("settings.audio.hp440"), &audioSettings.useFilter440);
            ImGui::Checkbox(tr("settings.audio.lp14k"), &audioSettings.useFilter14k);
        }

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
        renderBindingsSection("controls.emulation", 20, 5);
        renderBindingsSection("controls.nsf", 25, 3);

        ImGui::End();
    }

    void addTooltipToLastItem(const char *text) {
        if (!ImGui::IsItemHovered()) return;
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetIO().DisplaySize.x);
        ImGui::TextUnformatted(text);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }

    static constexpr int totalBindings = 28;
    static constexpr ImGuiWindowFlags WINDOW_FLAGS = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar;

    AppSettings *settings = nullptr;
    bool menuOpen = false;
    IMenuHandler *handler = nullptr;
    IInputContext *input = nullptr;

    bool controlsOpen = false;
    bool syncSettingsOpen = false;
    bool audioSettingsOpen = false;

    bool waitingForKey = false;
    bool bindAll = false;
    int bindingIndex = 0;
    int bindSectionBegin = 0;
    int bindSectionEnd = 0;
};