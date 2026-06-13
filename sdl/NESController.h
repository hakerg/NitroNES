#pragma once
#include <cstdint>
#include <SDL3/SDL.h>
#include "ControllerSettings.h"

class NESController {
public:
	ControllerSettings keys;

	explicit NESController(ControllerSettings settings) : keys(settings) {}

	void attachGamepad(SDL_Gamepad* gp)   { gamepad_ = gp; }
	void detachGamepad()                   { gamepad_ = nullptr; }
	void closeAndDetachGamepad()           { if (gamepad_) { SDL_CloseGamepad(gamepad_); gamepad_ = nullptr; } }
	SDL_Gamepad*   gamepad()   const       { return gamepad_; }
	SDL_JoystickID gamepadID() const       { return gamepad_ ? SDL_GetGamepadID(gamepad_) : 0; }

	uint8_t readState() const {
		int numKeys = 0;
		const bool* ks = SDL_GetKeyboardState(&numKeys);
		if (!ks) return 0x00;

		auto isDown = [&](SDL_Scancode sc) -> bool {
			return sc != SDL_SCANCODE_UNKNOWN && sc < (SDL_Scancode)numKeys && ks[sc];
		};
		auto padBtn = [&](SDL_GamepadButton btn) -> bool {
			return gamepad_ && SDL_GetGamepadButton(gamepad_, btn);
		};
		auto edge = [](bool down, bool& prev, bool& phase) {
			if (!down)      { prev = false; phase = false; }
			else if (!prev) { prev = true;  phase = true;  }
		};

		bool turboAdown = isDown(keys.key_TurboA) || padBtn(SDL_GAMEPAD_BUTTON_WEST);
		bool turboBdown = isDown(keys.key_TurboB) || padBtn(SDL_GAMEPAD_BUTTON_NORTH);
		edge(turboAdown, turboPrevA, turboPhaseA);
		edge(turboBdown, turboPrevB, turboPhaseB);

		uint8_t state = 0;
		auto btn = [&](SDL_Scancode sc, uint8_t bit) { if (isDown(sc)) state |= bit; };

		btn(keys.key_A,      0x80);
		btn(keys.key_B,      0x40);
		btn(keys.key_Select, 0x20);
		btn(keys.key_Start,  0x10);
		btn(keys.key_Up,     0x08);
		btn(keys.key_Down,   0x04);
		btn(keys.key_Left,   0x02);
		btn(keys.key_Right,  0x01);

		if (padBtn(SDL_GAMEPAD_BUTTON_SOUTH))      state |= 0x80;
		if (padBtn(SDL_GAMEPAD_BUTTON_EAST))       state |= 0x40;
		if (padBtn(SDL_GAMEPAD_BUTTON_BACK))       state |= 0x20;
		if (padBtn(SDL_GAMEPAD_BUTTON_START))      state |= 0x10;
		if (padBtn(SDL_GAMEPAD_BUTTON_DPAD_UP))    state |= 0x08;
		if (padBtn(SDL_GAMEPAD_BUTTON_DPAD_DOWN))  state |= 0x04;
		if (padBtn(SDL_GAMEPAD_BUTTON_DPAD_LEFT))  state |= 0x02;
		if (padBtn(SDL_GAMEPAD_BUTTON_DPAD_RIGHT)) state |= 0x01;

		if (turboPhaseA) state |= 0x80;
		if (turboPhaseB) state |= 0x40;

		return state;
	}

	void tickFrame() {
		int numKeys = 0;
		const bool* ks = SDL_GetKeyboardState(&numKeys);
		auto isDown = [&](SDL_Scancode sc) -> bool {
			return sc != SDL_SCANCODE_UNKNOWN && sc < (SDL_Scancode)numKeys && ks && ks[sc];
		};

		bool turboAdown = isDown(keys.key_TurboA) || (gamepad_ && SDL_GetGamepadButton(gamepad_, SDL_GAMEPAD_BUTTON_WEST));
		bool turboBdown = isDown(keys.key_TurboB) || (gamepad_ && SDL_GetGamepadButton(gamepad_, SDL_GAMEPAD_BUTTON_NORTH));
		if (turboAdown && turboPrevA) turboPhaseA = !turboPhaseA;
		if (turboBdown && turboPrevB) turboPhaseB = !turboPhaseB;
	}

private:
	SDL_Gamepad*   gamepad_    = nullptr;
	mutable bool   turboPhaseA = false;
	mutable bool   turboPhaseB = false;
	mutable bool   turboPrevA  = false;
	mutable bool   turboPrevB  = false;
};
