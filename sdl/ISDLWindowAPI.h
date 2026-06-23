#pragma once
#include <SDL3/SDL.h>
#include <string>

class ISDLWindowAPI {
public:
    virtual ~ISDLWindowAPI() = default;

    // Metoda przyjmuje wskaźnik na okno i zwraca true, jeśli na danej
    // platformie udało się pobrać sprzętowy scanline.
    virtual bool getScanLine(SDL_Window *window, int &outRaw) = 0;

    // Otwiera systemowe okno dialogowe wyboru pliku ROM.
    // Zwraca wybraną ścieżkę lub pusty string jeśli anulowano.
    virtual std::string openFileDialog(SDL_Window *parent) { return ""; }
};