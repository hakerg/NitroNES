#pragma once
#include "NESConst.h"

// Narzędzia do konwersji między współrzędnymi okna a przestrzenią pikseli NES.
//
// Przestrzeń NES:  x w [0, SCREEN_WIDTH),  y w [OVERSCAN_TOP, OVERSCAN_TOP + VISIBLE_H)
// Przestrzeń okna: x w [0, winW),          y w [0, winH)  (lewy górny róg = (0,0))
//
// Obraz NES jest wyśrodkowany w oknie z zachowaniem PAR (8:7) i bez overscan.

namespace NES {

	// Oblicza prostokąt docelowy (w pikselach okna) na który mapowany jest widoczny obszar NES.
	// dstX, dstY – lewy górny róg prostokąta w oknie
	// dstW, dstH – rozmiar prostokąta
	inline void calcDestRect(int winW, int winH,
							 float& dstX, float& dstY, float& dstW, float& dstH) {
		const float targetAspect =
			(float)(SCREEN_WIDTH * PAR_NUM) / (float)(VISIBLE_H * PAR_DEN);
		dstW = (float)winW;
		dstH = dstW / targetAspect;
		if (dstH > (float)winH) { dstH = (float)winH; dstW = dstH * targetAspect; }
		dstX = ((float)winW - dstW) * 0.5f;
		dstY = ((float)winH - dstH) * 0.5f;
	}

	// Konwertuje współrzędne okna (wx, wy) na współrzędne NES (nx, ny).
	// nx jest w przestrzeni [0, SCREEN_WIDTH), ny w [OVERSCAN_TOP, OVERSCAN_TOP + VISIBLE_H).
	// Zwraca false jeśli punkt leży poza obszarem obrazu NES.
	inline bool windowToNES(int winW, int winH, float wx, float wy, float& nx, float& ny) {
		float dstX, dstY, dstW, dstH;
		calcDestRect(winW, winH, dstX, dstY, dstW, dstH);

		if (wx < dstX || wx >= dstX + dstW || wy < dstY || wy >= dstY + dstH) return false;

		nx = (wx - dstX) / dstW * (float)SCREEN_WIDTH;
		ny = (wy - dstY) / dstH * (float)VISIBLE_H + (float)OVERSCAN_TOP;
		return true;
	}

	// Konwertuje współrzędne NES (nx, ny) na współrzędne okna (wx, wy).
	// nx w [0, SCREEN_WIDTH), ny w [OVERSCAN_TOP, OVERSCAN_TOP + VISIBLE_H).
	inline void nesToWindow(int winW, int winH, float nx, float ny, float& wx, float& wy) {
		float dstX, dstY, dstW, dstH;
		calcDestRect(winW, winH, dstX, dstY, dstW, dstH);

		wx = dstX + (nx / (float)SCREEN_WIDTH) * dstW;
		wy = dstY + ((ny - (float)OVERSCAN_TOP) / (float)VISIBLE_H) * dstH;
	}

} // namespace NES
