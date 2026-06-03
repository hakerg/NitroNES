#pragma once
#include <cstdint>
#include <SDL3/SDL.h>

// ============================================================
//  NESController — fizyczny kontroler NES mapowany na klawiaturę
//  i/lub pada Xbox (SDL3 Gamepad API).
//
//  Stan przycisków odczytywany jest BEZPOŚREDNIO z tablicy SDL
//  (SDL_GetKeyboardState) w momencie gdy CPU czyta rejestr $4016.
//  Jeśli podłączony jest pad (sdlGamepad != nullptr), jego stan
//  jest łączony (OR) z klawiaturą.
//
//  Mapowanie pada Xbox → NES (layout Pegasusa):
//    South (A)  = NES A
//    East  (B)  = NES B
//    West  (X)  = Turbo B
//    North (Y)  = Turbo A
//    Back       = Select
//    Start      = Start
//    D-pad      = kierunki
//
//  Domyślne mapowanie klawiszy:
//    A        = Num2
//    B        = Num3
//    Turbo A  = Num5   (auto-fire A ~30 Hz)
//    Turbo B  = Num6   (auto-fire B ~30 Hz)
//    Select   = Space
//    Start    = Enter
//    Up       = W
//    Down     = S
//    Left     = A
//    Right    = D
// ============================================================
class NESController {
public:
	// Mapowania klawiszy — zmień wedle potrzeby
	SDL_Scancode key_A       = SDL_SCANCODE_KP_2;
	SDL_Scancode key_B       = SDL_SCANCODE_KP_3;
	SDL_Scancode key_TurboA  = SDL_SCANCODE_KP_5;
	SDL_Scancode key_TurboB  = SDL_SCANCODE_KP_6;
	SDL_Scancode key_Select  = SDL_SCANCODE_SPACE;
	SDL_Scancode key_Start   = SDL_SCANCODE_RETURN;
	SDL_Scancode key_Up      = SDL_SCANCODE_W;
	SDL_Scancode key_Down    = SDL_SCANCODE_S;
	SDL_Scancode key_Left    = SDL_SCANCODE_A;
	SDL_Scancode key_Right   = SDL_SCANCODE_D;

	// Podepnij/odpnij pada SDL (nullptr = tylko klawiatura).
	void attachGamepad(SDL_Gamepad* gp) { sdlGamepad = gp; }
	void detachGamepad()                { sdlGamepad = nullptr; }
	void closeAndDetachGamepad()        { if (sdlGamepad) { SDL_CloseGamepad(sdlGamepad); sdlGamepad = nullptr; } }
	SDL_Gamepad* gamepad() const        { return sdlGamepad; }
	SDL_JoystickID gamepadID() const    { return sdlGamepad ? SDL_GetGamepadID(sdlGamepad) : 0; }

	// Zwróć bieżący stan wszystkich przycisków jako bajt NES:
	//   bit 7: A  |  bit 6: B  |  bit 5: Select  |  bit 4: Start
	//   bit 3: Up |  bit 2: Down | bit 1: Left   |  bit 0: Right
	// Odczyt jest bezpośredni z SDL — bez żadnego buforowania.
	// Wcisniecie/puszczenie turbo reaguje NATYCHMIAST (zbocze obslugiwane
	// tutaj). Rytm auto-fire (toggle) liczy tickFrame() raz na klatke,
	// dzieki czemu wielokrotne strobowanie $4016 nie psuje czestotliwosci.
	uint8_t readState() const {
		int numKeys = 0;
		const bool* keys = SDL_GetKeyboardState(&numKeys);
		if (!keys) return 0x00;

		auto isDown = [&](SDL_Scancode sc) -> bool {
			return sc < (SDL_Scancode)numKeys && keys[sc];
		};

		// Pomocnik: przycisk pada (zwraca false gdy brak pada)
		auto padBtn = [&](SDL_GamepadButton btn) -> bool {
			return sdlGamepad && SDL_GetGamepadButton(sdlGamepad, btn);
		};

		// Natychmiastowa reakcja na zbocze klawisza turbo:
		//   - press  -> phase=true (pierwsza ramka ZAWSZE aktywna),
		//   - release-> phase=false (zadnego "doklejonego" strzalu).
		// Powtorne wywolania w tej samej klatce (kolejne stroby $4016) NIE
		// zmieniaja phase - prev juz jest true, wiec nic sie nie dzieje.
		auto edge = [](bool down, bool& prev, bool& phase) {
			if (!down) {
				prev  = false;
				phase = false;
			} else if (!prev) {
				prev  = true;
				phase = true;
			}
		};
		bool turboAdown = isDown(key_TurboA) || padBtn(SDL_GAMEPAD_BUTTON_WEST);
		bool turboBdown = isDown(key_TurboB) || padBtn(SDL_GAMEPAD_BUTTON_NORTH);
		edge(turboAdown, turboPrevA, turboPhaseA);
		edge(turboBdown, turboPrevB, turboPhaseB);

		uint8_t state = 0;
		auto btn = [&](SDL_Scancode sc, uint8_t bit) {
			if (isDown(sc)) state |= bit;
		};

		btn(key_A,      0x80);
		btn(key_B,      0x40);
		btn(key_Select, 0x20);
		btn(key_Start,  0x10);
		btn(key_Up,     0x08);
		btn(key_Down,   0x04);
		btn(key_Left,   0x02);
		btn(key_Right,  0x01);

		// Pad Xbox (OR z klawiaturą)
		if (padBtn(SDL_GAMEPAD_BUTTON_SOUTH))           state |= 0x80; // A  = NES A
		if (padBtn(SDL_GAMEPAD_BUTTON_EAST))            state |= 0x40; // B  = NES B
		if (padBtn(SDL_GAMEPAD_BUTTON_BACK))            state |= 0x20; // Back   = Select
		if (padBtn(SDL_GAMEPAD_BUTTON_START))           state |= 0x10; // Start  = Start
		if (padBtn(SDL_GAMEPAD_BUTTON_DPAD_UP))         state |= 0x08;
		if (padBtn(SDL_GAMEPAD_BUTTON_DPAD_DOWN))       state |= 0x04;
		if (padBtn(SDL_GAMEPAD_BUTTON_DPAD_LEFT))       state |= 0x02;
		if (padBtn(SDL_GAMEPAD_BUTTON_DPAD_RIGHT))      state |= 0x01;

		if (turboPhaseA) state |= 0x80; // X (West)  = Turbo A
		if (turboPhaseB) state |= 0x40; // Y (North) = Turbo B

		return state;
	}

	// Wolane DOKLADNIE raz na klatke PPU (po consumeFrame()). Generuje rytm
	// auto-fire turbo dla TRZYMANYCH klawiszy. Zbocza (press/release) sa
	// obslugiwane natychmiast w readState() - tu tylko alternujemy fazy.
	void tickFrame() {
		bool turboAdown = isKeyDown(key_TurboA) ||
			(sdlGamepad && SDL_GetGamepadButton(sdlGamepad, SDL_GAMEPAD_BUTTON_WEST));
		bool turboBdown = isKeyDown(key_TurboB) ||
			(sdlGamepad && SDL_GetGamepadButton(sdlGamepad, SDL_GAMEPAD_BUTTON_NORTH));
		if (turboAdown && turboPrevA)
			turboPhaseA = !turboPhaseA;
		if (turboBdown && turboPrevB)
			turboPhaseB = !turboPhaseB;
	}

private:
	bool isKeyDown(SDL_Scancode sc) const {
		int numKeys = 0;
		const bool* keys = SDL_GetKeyboardState(&numKeys);
		if (!keys) return false;
		return sc < (SDL_Scancode)numKeys && keys[sc];
	}

	SDL_Gamepad* sdlGamepad  = nullptr;
	mutable bool turboPhaseA = false;
	mutable bool turboPhaseB = false;
	mutable bool turboPrevA  = false;  // poprzedni stan klawisza — do detekcji zbocza
	mutable bool turboPrevB  = false;
};
