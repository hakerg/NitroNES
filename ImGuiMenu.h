#pragma once
#include "imgui.h"
#include "backends/imgui_impl_sdl3.h"
#include "backends/imgui_impl_sdlrenderer3.h"
#include "NESCoreBase.h"
#include <SDL3/SDL.h>
#include <functional>

class ImGuiMenu {
private:
    SDL_Renderer* renderer;
    NESCoreBase*  core;
    bool open = false;

public:
    bool* allowScanlineSyncPtr = nullptr;
    bool* vsyncPtr = nullptr;
    bool* matchRefreshRatePtr = nullptr;
    int* volumePtr = nullptr;
    int* scanlineBufferMsPtr = nullptr;

    ImGuiMenu(SDL_Window* window, SDL_Renderer* renderer, NESCoreBase* core,
              bool* allowScanlineSync, bool* vsync,
              bool* matchRefreshRate, int* volume, int* scanlineBufferMs)
        : renderer(renderer), core(core),
          allowScanlineSyncPtr(allowScanlineSync), vsyncPtr(vsync),
          matchRefreshRatePtr(matchRefreshRate), volumePtr(volume),
          scanlineBufferMsPtr(scanlineBufferMs) {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui::StyleColorsDark();

        ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
        ImGui_ImplSDLRenderer3_Init(renderer);

        ImGuiIO& io = ImGui::GetIO();
        io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\arial.ttf", 20.0f);
        io.Fonts->Build();
    }

    bool isOpen() const { return open; }

    void processEvent(const SDL_Event* event) {
        ImGui_ImplSDL3_ProcessEvent(event);
    }

    void render() {
        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        open = false;

        if (ImGui::BeginMainMenuBar()) {
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(ImGui::GetStyle().FramePadding.x, 0));

            if (ImGui::BeginMenu("Plik")) {
                open = true;
                if (ImGui::MenuItem("Otwórz...")) {}
                if (ImGui::MenuItem("Przeładuj")) {}
                ImGui::Separator();
                if (ImGui::MenuItem("Wyjdź")) {}
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Emulacja")) {
                open = true;
                if (ImGui::BeginMenu("Podsystem")) {
                    int sub = core->pal ? 1 : 0;
                    if (ImGui::RadioButton("NTSC", &sub, 0)) core->pal = false;
                    if (ImGui::RadioButton("PAL",  &sub, 1)) core->pal = true;
                    ImGui::EndMenu();
                }
                ImGui::Separator();
                ImGui::Checkbox("Pauza", &core->paused);
                if (ImGui::MenuItem("Reset")) {}
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Ustawienia")) {
                open = true;
                if (ImGui::BeginMenu("Synchronizacja")) {
                    ImGui::BeginDisabled(*allowScanlineSyncPtr);
                    if (ImGui::Checkbox("Synchronizacja pionowa", vsyncPtr)) {
                        SDL_SetRenderVSync(renderer, *vsyncPtr ? 1 : 0);
                    }
                    ImGui::Checkbox("Dopasuj szybkość do częstotliwości monitora", matchRefreshRatePtr);
                    ImGui::EndDisabled();

                    if (ImGui::BeginMenu("Dopasowanie linii skanowania")) {
                        if (ImGui::Checkbox("Włączone", allowScanlineSyncPtr)) {
                            *vsyncPtr = false;
                            *matchRefreshRatePtr = true;
                        }

                        ImGui::BeginDisabled(!*allowScanlineSyncPtr);
                        ImGui::SliderInt("Bufor [ms]", scanlineBufferMsPtr, 0, 20);
                        ImGui::EndDisabled();

                        ImGui::EndMenu();
                    }
                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu("Dźwięk")) {
                    ImGui::SliderInt("Głośność", volumePtr, 0, 100);
                    ImGui::EndMenu();
                }
                if (ImGui::MenuItem("Sterowanie...")) {}
                ImGui::EndMenu();
            }

            ImGui::PopStyleVar();
            ImGui::EndMainMenuBar();
        }
        ImGui::Render();
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
    }

    void shutdown() {
        ImGui_ImplSDLRenderer3_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
    }
};