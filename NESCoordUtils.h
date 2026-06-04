#pragma once
#include "NESConst.h"

namespace NES {

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

	inline void windowToNES(int winW, int winH, float wx, float wy, float& nx, float& ny) {
		float dstX, dstY, dstW, dstH;
		calcDestRect(winW, winH, dstX, dstY, dstW, dstH);

		nx = (wx - dstX) / dstW * (float)SCREEN_WIDTH;
		ny = (wy - dstY) / dstH * (float)VISIBLE_H + (float)OVERSCAN_TOP;
	}

	inline void nesToWindow(int winW, int winH, float nx, float ny, float& wx, float& wy) {
		float dstX, dstY, dstW, dstH;
		calcDestRect(winW, winH, dstX, dstY, dstW, dstH);

		wx = dstX + (nx / (float)SCREEN_WIDTH) * dstW;
		wy = dstY + ((ny - (float)OVERSCAN_TOP) / (float)VISIBLE_H) * dstH;
	}

} // namespace NES
