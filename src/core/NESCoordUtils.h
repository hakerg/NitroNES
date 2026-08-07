#pragma once
#include "NESConst.h"

namespace NES {

inline void calcDestRect(int winW, int winH, float &dstX, float &dstY,
                         float &dstW, float &dstH,
                         NESStandard system = NESStandard::NTSC) {
    const int parNum = system == NESStandard::NTSC ? PAR_NUM_NTSC : PAR_NUM_PAL;
    const int parDen = system == NESStandard::NTSC ? PAR_DEN_NTSC : PAR_DEN_PAL;
    const float targetAspect =
        (float)(SCREEN_WIDTH * parNum) / (float)(SCREEN_HEIGHT * parDen);
    dstW = (float)winW;
    dstH = dstW / targetAspect;
    if (dstH > (float)winH) {
        dstH = (float)winH;
        dstW = dstH * targetAspect;
    }
    dstX = ((float)winW - dstW) * 0.5f;
    dstY = ((float)winH - dstH) * 0.5f;
}

} // namespace NES
