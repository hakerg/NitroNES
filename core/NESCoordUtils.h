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

} // namespace NES
