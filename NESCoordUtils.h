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

	inline void monitorToNES(int renderAreaX, int renderAreaY, int winW, int winH,
		float monitorX, float monitorY, float& nesX, float& nesY) {
		float dstX, dstY, dstW, dstH;
		calcDestRect(winW, winH, dstX, dstY, dstW, dstH);

		float localWx = monitorX - (float)renderAreaX;
		float localWy = monitorY - (float)renderAreaY;

		nesX = (localWx - dstX) / dstW * (float)SCREEN_WIDTH;
		nesY = (localWy - dstY) / dstH * (float)VISIBLE_H + (float)OVERSCAN_TOP;
	}

	inline void nesToMonitor(int renderAreaX, int renderAreaY, int winW, int winH,
		float nesX, float nesY, float& monitorX, float& monitorY) {

		float dstX, dstY, dstW, dstH;
		calcDestRect(winW, winH, dstX, dstY, dstW, dstH);

		float localWx = (nesX / (float)SCREEN_WIDTH) * dstW + dstX;
		float localWy = ((nesY - (float)OVERSCAN_TOP) / (float)VISIBLE_H) * dstH + dstY;

		monitorX = localWx + (float)renderAreaX;
		monitorY = localWy + (float)renderAreaY;
	}

} // namespace NES
