#include "NESConst.h"
#include "NESCoreBase.h"
#include "NSFPlayer.h"
#include "NESSystem.h"
#include "AudioStream.h"
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
#include "PrecisionSleeper.h"

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

static void toggleFullscreen(SDL_Window* window) {
	bool isFs = (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != 0;
	if (!isFs) {
		SDL_DisplayID display = SDL_GetDisplayForWindow(window);
		const SDL_DisplayMode* desktop = SDL_GetDesktopDisplayMode(display);
		int w = desktop ? desktop->w : 0;
		int h = desktop ? desktop->h : 0;
		int n = 0;
		SDL_DisplayMode** modes = SDL_GetFullscreenDisplayModes(display, &n);
		const SDL_DisplayMode* best = nullptr;
		float bestDiff = 1e9f;
		for (int i = 0; i < n; i++) {
			if (modes[i]->w != w || modes[i]->h != h) continue;
			float diff = std::fabs(modes[i]->refresh_rate - NES::REFRESH_NTSC);
			if (diff < bestDiff) { bestDiff = diff; best = modes[i]; }
		}
		if (best) SDL_SetWindowFullscreenMode(window, best);
		SDL_free(modes);
		SDL_HideCursor();
	} else {
		SDL_ShowCursor();
	}
	SDL_SetWindowFullscreen(window, !isFs);
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

	// 2. SDL + okno.
	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
		std::cerr << "Blad SDL3: " << SDL_GetError() << "\n";
		return 1;
	}

	int winW = 0, winH = 0;
	core->defaultWindowSize(winW, winH);

	// Single-buffer OpenGL: glFlush() bezposrednio aktualizuje front buffer
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 0);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);

	Uint32 winFlags = SDL_WINDOW_OPENGL;
	if (core->windowResizable()) winFlags |= SDL_WINDOW_RESIZABLE;
	SDL_Window* window = SDL_CreateWindow(core->windowTitle(filename).c_str(),
		winW, winH, winFlags);
	if (!window) { std::cerr << "Blad okna: " << SDL_GetError() << "\n"; SDL_Quit(); return 1; }

	SDL_GLContext glCtx = SDL_GL_CreateContext(window);
	if (!glCtx) {
		std::cerr << "Blad kontekstu GL: " << SDL_GetError() << "\n";
		SDL_DestroyWindow(window); SDL_Quit(); return 1;
	}
	SDL_GL_MakeCurrent(window, glCtx);
	SDL_GL_SetSwapInterval(0);

	core->initVideo(window);

	// 3. Audio.
	AudioStream audioStream;
	if (!audioStream.open()) std::cerr << "Ostrzezenie: brak audio\n";

	core->setHost({
		[&]{ audioStream.reset(); },
	});

	PrecisionSleeper sleeper;

	std::atomic<bool> running{ true };
	bool   paused    = false;
	double baseSpeed = 1.0;
	bool   keyFast   = false; // TAB
	bool   keySlow   = false; // Shift+TAB
	bool   padFast   = false; // RT
	bool   padSlow   = false; // LT

	auto updateBaseSpeed = [&]() {
		if      (keyFast || padFast) baseSpeed = NES::MAX_SPEED;
		else if (keySlow || padSlow) baseSpeed = 0.5;
		else                         baseSpeed = 1.0;
	};

	Uint64 freq = SDL_GetPerformanceFrequency();
	Uint64 prev = SDL_GetPerformanceCounter();
	double lag = 0.0;

	while (running.load(std::memory_order_relaxed)) {
		SDL_Event ev;
		while (SDL_PollEvent(&ev)) {
			// Globalne (okno/aplikacja) - reszta idzie do rdzenia.
			if (ev.type == SDL_EVENT_QUIT) { running = false; continue; }
			if (ev.type == SDL_EVENT_KEY_DOWN && !ev.key.repeat) {
				switch (ev.key.scancode) {
					case SDL_SCANCODE_ESCAPE: running = false;            continue;
					case SDL_SCANCODE_P:      paused  = !paused;          continue;
					case SDL_SCANCODE_F11:    toggleFullscreen(window);   continue;
					default: break;
				}
			}
			if (ev.type == SDL_EVENT_KEY_DOWN || ev.type == SDL_EVENT_KEY_UP) {
				if (ev.key.scancode == SDL_SCANCODE_TAB && !ev.key.repeat) {
					bool shift = (SDL_GetModState() & SDL_KMOD_SHIFT) != 0;
					bool down  = (ev.type == SDL_EVENT_KEY_DOWN);
					if (shift) { keySlow = down; keyFast = false; }
					else        { keyFast = down; keySlow = false; }
					updateBaseSpeed();
					continue;
				}
			}
			if (ev.type == SDL_EVENT_GAMEPAD_AXIS_MOTION) {
				constexpr Sint16 THRESHOLD = 8000;
				if (ev.gaxis.axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) {
					padFast = ev.gaxis.value > THRESHOLD;
					updateBaseSpeed(); continue;
				}
				if (ev.gaxis.axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER) {
					padSlow = ev.gaxis.value > THRESHOLD;
					updateBaseSpeed(); continue;
				}
			}
			core->handleEvent(ev, paused);
		}

		core->setSpeed(paused ? 0.0 : baseSpeed);

		Uint64 now = SDL_GetPerformanceCounter();
		double elapsed = (double)(now - prev) / (double)freq;
		lag += elapsed;
		if (lag > NES::MAX_DELAY) lag = NES::MAX_DELAY; // limit spiral-of-lag
		prev = now;

		double spd = core->getSpeed();
		if (spd > 0.0) {
			float sample = 0.0f;
			double dt = 0.0;
			while (lag > 0.0) {
				core->clockOneCycle(sample, dt);
				audioStream.addSample(dt, sample);
				lag -= dt;
			}
			audioStream.commitBatch();
		}

		sleeper.sleep(0.001);
	}

	audioStream.close();
	core->shutdown();
	SDL_GL_DestroyContext(glCtx);
	SDL_DestroyWindow(window);
	SDL_Quit();
	return 0;
}
