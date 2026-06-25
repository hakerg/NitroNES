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

        ImFontConfig fontConfig;
        fontConfig.FontDataOwnedByAtlas = false;
        fontConfig.GlyphOffset.y = -2.0f;
        fontConfig.ExtraSizeScale = 16.0f / 24.0f;

        ImGuiIO &io = ImGui::GetIO();
        io.Fonts->AddFontFromMemoryTTF((void*)PressStart2P_Regular, 116008, 24.0f, &fontConfig);
        io.Fonts->Build();
        io.IniFilename = nullptr;
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
        if (!handler->isVisible())
            return;

        NESCoreBase *core = session ? &session->core() : nullptr;

        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        menuOpen = false;

        if (ImGui::BeginMainMenuBar()) {
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                                ImVec2(ImGui::GetStyle().FramePadding.x, 0));

            if (ImGui::BeginMenu(tr("file"))) {
                menuOpen = true;
                if (ImGui::MenuItem(tr("file.open")))
                    handler->onOpen();
                if (session && ImGui::MenuItem(tr("file.reload")))
                    handler->onReload();
                if (session && ImGui::MenuItem(tr("file.close")))
                    handler->onClose();
                ImGui::Separator();
                if (ImGui::MenuItem(tr("file.quit")))
                    handler->onQuit();
                ImGui::EndMenu();
            }

            if (settings && core) {
                if (ImGui::BeginMenu(tr("emulation"))) {
                    menuOpen = true;
                    if (ImGui::BeginMenu(tr("emulation.system"))) {
                        int sub = core->pal ? 1 : 0;
                        if (ImGui::RadioButton("NTSC", &sub, 0))
                            core->pal = false;
                        if (ImGui::RadioButton("PAL", &sub, 1))
                            core->pal = true;
                        ImGui::EndMenu();
                    }
                    ImGui::Separator();
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
                ImGui::Separator();

                if (ImGui::MenuItem(tr("settings.sync")))
                    syncSettingsOpen = true;

                if (ImGui::MenuItem(tr("settings.audio")))
                    audioSettingsOpen = true;

                if (ImGui::MenuItem(tr("settings.controls")))
                    controlsOpen = true;

                ImGui::EndMenu();
            }

            ImGui::PopStyleVar();
            ImGui::EndMainMenuBar();
        }

        renderSyncWindow(renderer, session, baseSpeed);
        renderAudioWindow();
        renderControlsWindow();

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

    void alignedText(const char *text) {
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(text);
    }

    void alignedWarning(const char *text) {
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.4f, 1.0f), "%s", text);
    }

    void renderBindingRow(int index) {
        Binding binding = bindingAt(index);
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        alignedText(tr(binding.label));
        ImGui::TableNextColumn();
        alignedText(bindingName(binding).c_str());
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

        if (ImGui::BeginTable(
                nameId, 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn(tr("controls.action"));
            ImGui::TableSetupColumn(tr("controls.key"));
            ImGui::TableSetupColumn("");
            ImGui::TableHeadersRow();
            for (int i = 0; i < count; i++)
                renderBindingRow(begin + i);
            ImGui::EndTable();
        }

        alignedText("");
    }

    void renderSyncWindow(SDL_Renderer *renderer, IFileSession *session, double baseSpeed) {
        if (!syncSettingsOpen || !settings)
            return;

        menuOpen = true;
        if (!ImGui::Begin(tr("settings.sync"), &syncSettingsOpen, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::End();
            return;
        }

        ImGui::BeginDisabled(settings->allowScanlineSync);
        if (ImGui::Checkbox(tr("settings.vsync"), &settings->vsync)) {
            SDL_SetRenderVSync(renderer, settings->vsync ? 1 : 0);
        }
        ImGui::EndDisabled();

        alignedText("");

        int syncMode = 0;
        if (settings->allowScanlineSync) {
            syncMode = 2;
        } else if (settings->matchRefreshRate) {
            syncMode = 1;
        }

        ImGui::TextUnformatted(tr("settings.sync_mode"));

        const char* modeItems[] = {
            tr("settings.sync.none"),
            tr("settings.sync.timer"),
            tr("settings.sync.scanline")
        };

        if (ImGui::Combo("##sync_mode_combo", &syncMode, modeItems, IM_ARRAYSIZE(modeItems))) {
            if (syncMode == 0) {
                settings->matchRefreshRate = false;
                settings->allowScanlineSync = false;
            } else if (syncMode == 1) {
                settings->matchRefreshRate = true;
                settings->allowScanlineSync = false;
            } else if (syncMode == 2) {
                settings->matchRefreshRate = true;
                settings->allowScanlineSync = true;
                settings->vsync = false;
                if (renderer) {
                    SDL_SetRenderVSync(renderer, 0);
                }
            }
        }

        if (syncMode == 2) {
            ImGui::SliderInt(tr("settings.scanline.buffer"),
                             &settings->scanlineBufferMs, 0, 20);
        }

        if (session) {
            if (syncMode == 1) {
                renderRefreshRateStatus(session->canMatchRefreshRate(baseSpeed));
            } else if (syncMode == 2) {
                renderScanlineSyncStatus(session->canUseScanlineSync(baseSpeed));
            }

            alignedText("");

            double speed = session->core().speed;
            alignedText(
                std::format("{}: {:.2f}%", tr("settings.current_speed"),
                            speed * 100.0).c_str());
        }

        ImGui::End();
    }

    void renderRefreshRateStatus(CanMatchRefreshRateResult res) {
        switch (res) {
        case CanMatchRefreshRateResult::SystemError:
            alignedWarning(tr("status.system_error"));
            break;
        case CanMatchRefreshRateResult::RefreshRateOutsideTolerance:
            alignedWarning(tr("status.outside_tolerance"));
            break;
        default:
            break;
        }
    }

    void renderScanlineSyncStatus(CanUseScanlineSyncResult res) {
        switch (res) {
        case CanUseScanlineSyncResult::NoFullscreen:
            alignedWarning(tr("status.no_fullscreen"));
            break;
        case CanUseScanlineSyncResult::SystemError:
            alignedWarning(tr("status.system_error"));
            break;
        case CanUseScanlineSyncResult::RefreshRateOutsideTolerance:
            alignedWarning(tr("status.outside_tolerance"));
            break;
        default:
            break;
        }
    }

    void renderAudioWindow() {
        if (!audioSettingsOpen || !settings)
            return;

        menuOpen = true;
        if (!ImGui::Begin(tr("settings.audio"), &audioSettingsOpen, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::End();
            return;
        }

        AudioSettings &audioSettings = settings->audioSettings;
        ImGui::SliderFloat(tr("settings.volume"), &audioSettings.volume, 0.0f, 2.5f, "%.1f");
        ImGui::Checkbox(tr("settings.audio.reduce_clicks"), &audioSettings.reduceClicks);
        ImGui::Checkbox(tr("settings.audio.adjust_pitch"), &settings->adjustPitch);

        alignedText("");

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
        if (!ImGui::Begin(tr("controls.title"), &controlsOpen, ImGuiWindowFlags_AlwaysAutoResize)) {
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

    static constexpr int totalBindings = 28;
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