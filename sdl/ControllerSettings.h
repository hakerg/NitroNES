#pragma once
#include <SDL3/SDL.h>

struct ControllerSettings {
	SDL_Scancode key_A      = SDL_SCANCODE_UNKNOWN;
	SDL_Scancode key_B      = SDL_SCANCODE_UNKNOWN;
	SDL_Scancode key_TurboA = SDL_SCANCODE_UNKNOWN;
	SDL_Scancode key_TurboB = SDL_SCANCODE_UNKNOWN;
	SDL_Scancode key_Select = SDL_SCANCODE_UNKNOWN;
	SDL_Scancode key_Start  = SDL_SCANCODE_UNKNOWN;
	SDL_Scancode key_Up     = SDL_SCANCODE_UNKNOWN;
	SDL_Scancode key_Down   = SDL_SCANCODE_UNKNOWN;
	SDL_Scancode key_Left   = SDL_SCANCODE_UNKNOWN;
	SDL_Scancode key_Right  = SDL_SCANCODE_UNKNOWN;

	static ControllerSettings player1() {
		ControllerSettings s;
		s.key_A      = SDL_SCANCODE_KP_2;
		s.key_B      = SDL_SCANCODE_KP_3;
		s.key_TurboA = SDL_SCANCODE_KP_5;
		s.key_TurboB = SDL_SCANCODE_KP_6;
		s.key_Select = SDL_SCANCODE_SPACE;
		s.key_Start  = SDL_SCANCODE_RETURN;
		s.key_Up     = SDL_SCANCODE_R;
		s.key_Down   = SDL_SCANCODE_F;
		s.key_Left   = SDL_SCANCODE_D;
		s.key_Right  = SDL_SCANCODE_G;
		return s;
	}

	static ControllerSettings player2() {
		ControllerSettings s;
		s.key_A      = SDL_SCANCODE_Z;
		s.key_B      = SDL_SCANCODE_X;
		s.key_TurboA = SDL_SCANCODE_A;
		s.key_TurboB = SDL_SCANCODE_S;
		s.key_Select = SDL_SCANCODE_UNKNOWN;
		s.key_Start  = SDL_SCANCODE_UNKNOWN;
		s.key_Up     = SDL_SCANCODE_UP;
		s.key_Down   = SDL_SCANCODE_DOWN;
		s.key_Left   = SDL_SCANCODE_LEFT;
		s.key_Right  = SDL_SCANCODE_RIGHT;
		return s;
	}
};
