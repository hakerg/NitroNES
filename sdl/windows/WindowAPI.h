#pragma once
#include "../ISDLWindowAPI.h"
#include <windows.h>
#include "d3dkmthk.h"

#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")

class WindowAPI : public ISDLWindowAPI {
public:
	bool getScanLine(SDL_Window* window, int& outRaw) override {
		if (!window) return false;

		HWND hwnd = (HWND)SDL_GetPointerProperty(
			SDL_GetWindowProperties(window),
			SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
		if (!hwnd) return false;

		HMONITOR hmon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
		if (!hmon) return false;

		MONITORINFOEXA mi{};
		mi.cbSize = sizeof(mi);
		if (!GetMonitorInfoA(hmon, &mi)) return false;

		HDC hdc = CreateDCA(nullptr, mi.szDevice, nullptr, nullptr);
		if (!hdc) return false;

		D3DKMT_OPENADAPTERFROMHDC oa{};
		oa.hDc = hdc;
		if (D3DKMTOpenAdapterFromHdc(&oa) != 0) {
			DeleteDC(hdc);
			return false;
		}
		DeleteDC(hdc);

		D3DKMT_GETSCANLINE gs{};
		gs.hAdapter = oa.hAdapter;
		gs.VidPnSourceId = oa.VidPnSourceId;
		bool success = (D3DKMTGetScanLine(&gs) == 0);

		D3DKMT_CLOSEADAPTER ca{};
		ca.hAdapter = oa.hAdapter;
		D3DKMTCloseAdapter(&ca);

		if (success) {
			outRaw = (int)gs.ScanLine;
			return true;
		}
		return false;
	}
};