#pragma once
#include "../ISDLWindowAPI.h"
#include <windows.h>
#include <commdlg.h>
#include <string>
#include <filesystem>
#include "d3dkmthk.h"

#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "comdlg32.lib")

class WindowAPI : public ISDLWindowAPI {
public:
	std::string openFileDialog(SDL_Window* sdlWindow) override {
		HWND hwnd = sdlWindow
			? (HWND)SDL_GetPointerProperty(SDL_GetWindowProperties(sdlWindow),
				SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr)
			: nullptr;

		wchar_t buf[MAX_PATH] = {};
		OPENFILENAMEW ofn{};
		ofn.lStructSize  = sizeof(ofn);
		ofn.hwndOwner    = hwnd;
		ofn.lpstrFilter  = L"Pliki NES/NSF\0*.nes;*.nsf\0Wszystkie pliki\0*.*\0\0";
		ofn.lpstrFile    = buf;
		ofn.nMaxFile     = MAX_PATH;
		ofn.Flags        = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
		ofn.lpstrTitle   = L"Otwórz plik ROM";

		if (!GetOpenFileNameW(&ofn)) return "";

		auto u8 = std::filesystem::path(buf).u8string();
		return std::string(u8.begin(), u8.end());
	}

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