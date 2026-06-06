#include "NESConst.h"
#include "NESCoreBase.h"
#include "NSFPlayer.h"
#include "NESSystem.h"
#include "SDLAudioStream.h"
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
#include "SyncStrategy.h"
#include "TimerSyncStrategy.h"
#include "ScanlineSyncStrategy.h"
#include "AudioCallbackSyncStrategy.h"

#pragma comment(lib, "winmm.lib")

// Magic bytes: "NES\x1A" = iNES, "NESM\x1A" = NSF.
static bool isNesRomFile(const std::string& path) {
	std::ifstream ifs(path, std::ios::binary);
	if (!ifs) return false;
	char m[5] = {};
	ifs.read(m, 5);
	if (ifs.gcount() < 4) return false;
	if (m[0] == 'N' && m[1] == 'E' && m[2] == 'S' && m[3] == 'M') return false;
	return m[0] == 'N' && m[1] == 'E' && m[2] == 'S' && m[3] == 0x1A;
}

int wmain(int argc, wchar_t* argv[])
{
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

	// 1. Wybor rdzenia.
	std::unique_ptr<NESCoreBase> core = isNesRomFile(romPath)
		? std::unique_ptr<NESCoreBase>(std::make_unique<NESSystem>())
		: std::unique_ptr<NESCoreBase>(std::make_unique<NSFPlayer>());

	if (!core->loadFile(romPath)) return 1;

	// Wymagane do scanline sync, żeby ClientToScreen i GetMonitorInfo zwracały fizyczne piksele
	SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

	// 2. SDL + okno.
	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
		std::cerr << "Blad SDL3: " << SDL_GetError() << "\n";
		return 1;
	}

	int winW = 0, winH = 0;
	core->defaultWindowSize(winW, winH);

	Uint32 winFlags = 0;
	if (core->windowResizable()) winFlags |= SDL_WINDOW_RESIZABLE;
	SDL_Window* window = SDL_CreateWindow(core->windowTitle(filename).c_str(), winW, winH, winFlags);
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
			SDL_RenderPresent(renderer);
		};
	}

	// 3. Audio.
	SDLAudioStream audioStream;
	if (!audioStream.open()) std::cerr << "Ostrzezenie: brak audio\n";

	core->onAudioSample = [&](float sample, double dt) {
		audioStream.addNESSample(dt, sample);
	};

	bool running = true;
	bool keyFast = false; // TAB
	bool keySlow = false; // Shift+TAB
	bool padFast = false; // RT
	bool padSlow = false; // LT
	SDL_JoystickID speedPadId = 0; // pad aktualnie kontrolujący prędkość (0 = brak)

	Uint64 lastMouseMoveTime = SDL_GetTicks();
	bool cursorVisible = true;

	auto calcSpeed = [&]() -> double {
		if      (keyFast || padFast) return 4.0;
		else if (keySlow || padSlow) return 0.5;
		else                         return 1.0;
	};

	auto processEvent = [&](const SDL_Event& ev) {
		if (ev.type == SDL_EVENT_MOUSE_MOTION) {
			lastMouseMoveTime = SDL_GetTicks();
			if (!cursorVisible) {
				SDL_ShowCursor();
				cursorVisible = true;
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
			case SDL_SCANCODE_ESCAPE: running = false;            return;
			case SDL_SCANCODE_P:      core->togglePause();        return;
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
			if (speedPadId == 0) speedPadId = jid;      // pierwszy pad, który ruszy trigger, przejmuje kontrolę
			if (speedPadId != jid) return;              // inny pad – ignoruj
			if (ev.gaxis.axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) padFast = ev.gaxis.value > THRESHOLD;
			if (ev.gaxis.axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER)  padSlow = ev.gaxis.value > THRESHOLD;
			if (!padFast && !padSlow) speedPadId = 0;   // oba triggery puszczone – zwalniamy blokadę
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

	// Przygotowanie kontekstu dla strategii synchronizacji
	SyncContext syncContext{
		.core = core.get(),
		.audioStream = &audioStream,
		.window = window,
		.handleEvent = processEvent,
		.getSpeed = calcSpeed
	};

	audioStream.bindContext(&syncContext);

	TimerSyncStrategy timerStrategy(syncContext, NES::MAX_LAG);
	ScanlineSyncStrategy scanlineStrategy(syncContext);
	AudioCallbackSyncStrategy audioStrategy(syncContext);

	while (running) {
		SyncStrategy* activeStrategy;

		if (core->hasPPU()) {
			if (scanlineStrategy.canUse()) {
				activeStrategy = static_cast<SyncStrategy*>(&scanlineStrategy);
			}
			else {
				activeStrategy = &timerStrategy;
			}
		}
		else {
			activeStrategy = &audioStrategy;
		}

		activeStrategy->run();

		if (cursorVisible && SDL_GetTicks() - lastMouseMoveTime >= 1000) {
			SDL_HideCursor();
			cursorVisible = false;
		}
	}

	audioStream.close();
	core->shutdown();

	SDL_DestroyTexture(texture);
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();

	return 0;
}