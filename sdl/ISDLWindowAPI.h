#pragma once
#include <SDL3/SDL.h>

class ISDLWindowAPI {
public:
	virtual ~ISDLWindowAPI() = default;

	// Metoda przyjmuje wskaźnik na okno i zwraca true, jeśli na danej
	// platformie udało się pobrać sprzętowy scanline.
	virtual bool getScanLine(SDL_Window* window, int& outRaw) = 0;
};