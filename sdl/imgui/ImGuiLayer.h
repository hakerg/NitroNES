#pragma once
#include <SDL3/SDL.h>
#include <format>
#include <string>
#include "imgui.h"
#include "backends/imgui_impl_sdl3.h"
#include "backends/imgui_impl_sdlrenderer3.h"
#include "../../IMenuHandler.h"
#include "../../AppSettings.h"
#include "../../lang/LanguageRegistry.h"
#include "../../IFileSession.h"

class ImGuiLayer {
public:
	ImGuiLayer(SDL_Window* window, SDL_Renderer* renderer) {
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGui::StyleColorsDark();

		ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
		ImGui_ImplSDLRenderer3_Init(renderer);

		ImGuiIO& io = ImGui::GetIO();
		io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\arial.ttf", 20.0f);
		io.Fonts->Build();
		io.IniFilename = nullptr;
	}

	~ImGuiLayer() {
		ImGui_ImplSDLRenderer3_Shutdown();
		ImGui_ImplSDL3_Shutdown();
		ImGui::DestroyContext();
	}

	void init(AppSettings* s, IMenuHandler* h) {
		settings = s;
		handler = h;
	}

	void processEvent(const SDL_Event* ev) {
		ImGui_ImplSDL3_ProcessEvent(ev);
	}

	bool isMenuOpen() const { return menuOpen; }

	void render(SDL_Renderer* renderer, IFileSession& session) {
		if (!handler->isVisible()) return;

		NESCoreBase& core = session.core();

		ImGui_ImplSDLRenderer3_NewFrame();
		ImGui_ImplSDL3_NewFrame();
		ImGui::NewFrame();

		menuOpen = false;

		if (ImGui::BeginMainMenuBar()) {
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(ImGui::GetStyle().FramePadding.x, 0));

			if (ImGui::BeginMenu(tr("file"))) {
				menuOpen = true;
				if (ImGui::MenuItem(tr("file.open"))) handler->onOpen();
				if (ImGui::MenuItem(tr("file.reload"))) handler->onReload();
				ImGui::Separator();
				if (ImGui::MenuItem(tr("file.quit"))) handler->onQuit();
				ImGui::EndMenu();
			}

			if (settings) {
				if (ImGui::BeginMenu(tr("emulation"))) {
					menuOpen = true;
					if (ImGui::BeginMenu(tr("emulation.system"))) {
						int sub = core.pal ? 1 : 0;
						if (ImGui::RadioButton("NTSC", &sub, 0)) core.pal = false;
						if (ImGui::RadioButton("PAL", &sub, 1)) core.pal = true;
						ImGui::EndMenu();
					}
					ImGui::Separator();
					ImGui::Checkbox(tr("emulation.pause"), &core.paused);
					if (ImGui::MenuItem(tr("emulation.reset"))) handler->onReset();

					ImGui::EndMenu();
				}
			}

			if (settings && ImGui::BeginMenu(tr("settings"))) {
				menuOpen = true;
				if (ImGui::BeginMenu(tr("settings.language"))) {
					auto& reg = LanguageRegistry::instance();
					for (int i = 0; i < (int)reg.languages().size(); i++) {
						ImGui::RadioButton(reg.languages()[i]->getName(), &reg.currentIndex, i);
					}
					ImGui::EndMenu();
				}
				ImGui::Separator();
				if (ImGui::BeginMenu(tr("settings.sync"))) {
					ImGui::BeginDisabled(settings->allowScanlineSync);
					if (ImGui::Checkbox(tr("settings.vsync"), &settings->vsync)) {
						SDL_SetRenderVSync(renderer, settings->vsync ? 1 : 0);
					}
					{
						double speed = session.calcSpeedMultiplier(1.0);
						std::string matchLabel = speed > 0.0
							? std::format("{} ({:.2f}%)", tr("settings.match_hz"), speed * 100.0)
							: std::string(tr("settings.match_hz"));
						ImGui::Checkbox(matchLabel.c_str(), &settings->matchRefreshRate);
					}
					ImGui::EndDisabled();

					if (settings->scanlineBufferMs) {
						if (ImGui::BeginMenu(tr("settings.scanline"))) {
							if (ImGui::Checkbox(tr("settings.scanline.enabled"), &settings->allowScanlineSync)) {
								settings->vsync = false;
								settings->matchRefreshRate = true;
							}
							ImGui::BeginDisabled(!settings->allowScanlineSync);
							ImGui::SliderInt(tr("settings.scanline.buffer"), &settings->scanlineBufferMs, 0, 20);
							ImGui::EndDisabled();
							ImGui::EndMenu();
						}
					}
					ImGui::EndMenu();
				}
				if (ImGui::BeginMenu(tr("settings.audio"))) {
					AudioSettings& audioSettings = settings->audioSettings;
					ImGui::SliderFloat(tr("settings.volume"), &audioSettings.volume, 0.0f, 2.5f);
					if (ImGui::BeginMenu(tr("settings.audio.filters"))) {
						ImGui::Checkbox(tr("settings.audio.hp90"), &audioSettings.useFilter90);
						ImGui::Checkbox(tr("settings.audio.hp440"), &audioSettings.useFilter440);
						ImGui::Checkbox(tr("settings.audio.lp14k"), &audioSettings.useFilter14k);
						ImGui::EndMenu();
					}
					ImGui::EndMenu();
				}
				if (ImGui::MenuItem(tr("settings.controls"))) {}
				ImGui::EndMenu();
			}

			ImGui::PopStyleVar();
			ImGui::EndMainMenuBar();
		}

		ImGui::Render();
		ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
	}

private:
	AppSettings* settings = nullptr;
	bool menuOpen = false;
	IMenuHandler* handler = nullptr;
};