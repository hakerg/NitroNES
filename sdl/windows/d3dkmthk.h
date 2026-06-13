#pragma once
// Minimalny stub d3dkmthk.h dla kompilatorów MinGW (brak w nagłówkach WDK)
// Zawiera tylko typy i funkcje używane w WindowAPI.h

#ifndef _D3DKMTHK_H_
#define _D3DKMTHK_H_

#include <windows.h>

#ifndef D3DKMT_HANDLE
typedef UINT D3DKMT_HANDLE;
#endif

#ifndef D3DDDI_VIDEO_PRESENT_SOURCE_ID
typedef UINT D3DDDI_VIDEO_PRESENT_SOURCE_ID;
#endif

typedef struct _D3DKMT_OPENADAPTERFROMHDC {
    HDC                        hDc;
    D3DKMT_HANDLE              hAdapter;
    LUID                       AdapterLuid;
    D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId;
} D3DKMT_OPENADAPTERFROMHDC;

typedef struct _D3DKMT_GETSCANLINE {
    D3DKMT_HANDLE              hAdapter;
    D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId;
    BOOL                       InVerticalBlank;
    UINT                       ScanLine;
} D3DKMT_GETSCANLINE;

typedef struct _D3DKMT_CLOSEADAPTER {
    D3DKMT_HANDLE              hAdapter;
} D3DKMT_CLOSEADAPTER;

#ifdef __cplusplus
extern "C" {
#endif

NTSTATUS WINAPI D3DKMTOpenAdapterFromHdc(D3DKMT_OPENADAPTERFROMHDC* pData);
NTSTATUS WINAPI D3DKMTGetScanLine(D3DKMT_GETSCANLINE* pData);
NTSTATUS WINAPI D3DKMTCloseAdapter(const D3DKMT_CLOSEADAPTER* pData);

#ifdef __cplusplus
}
#endif

#endif // _D3DKMTHK_H_

