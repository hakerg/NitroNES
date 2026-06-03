#pragma once
#include <windows.h>
#include <d3dkmthk.h>
#include <wingdi.h>
#include <vector>
#include <cstring>

#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")

// Niezalezny od emulatora detektor refresh rate aktywnego monitora.
// Bez watku tla - stan jest odswiezany leniwie przy wywolaniu getScanLine()
// lub getRefreshHz(). Wykrywa zmiane monitora pod sledzonym oknem i wtedy:
//   - ponownie otwiera adapter (dla D3DKMTGetScanLine),
//   - odpytuje CCD (QueryDisplayConfig) o dokladny DISPLAYCONFIG_RATIONAL
//     (Numerator/Denominator) refresh rate'u aktywnej sciezki.
//
// Uwaga: klasa nie jest thread-safe. Zakladamy konsumenta z jednego watku
// (np. BeamRacer w petli audio/emulacji).
class MonitorRefreshRateDetector {
public:
	MonitorRefreshRateDetector() = default;

	~MonitorRefreshRateDetector() { closeAdapter(); }

	MonitorRefreshRateDetector(const MonitorRefreshRateDetector&) = delete;
	MonitorRefreshRateDetector& operator=(const MonitorRefreshRateDetector&) = delete;

	// Okno, ktorego monitor sledzimy. Mozna zmieniac w runtime.
	void setWindow(HWND hwnd) { trackedHwnd = hwnd; }

	// Bezposrednie odczytanie aktualnej pozycji wiazki monitora.
	// Zwraca false jesli adapter nie jest otwarty lub zapytanie sie nie udalo.
	bool getScanLine(int& outRaw) const {
		ensureCurrentMonitor();
		if (!adapterHandle) return false;

		D3DKMT_GETSCANLINE gs{};
		gs.hAdapter      = adapterHandle;
		gs.VidPnSourceId = vidPnSourceId;
		if (D3DKMTGetScanLine(&gs) != 0) return false;

		outRaw           = (int)gs.ScanLine;
		return true;
	}

	// Zwraca refresh rate (Hz) z CCD lub 0 jesli niedostepny.
	double getRefreshHz() const {
		ensureCurrentMonitor();
		return refreshHz;
	}

private:
	void ensureCurrentMonitor() const {
		HMONITOR hmon = trackedHwnd
			? MonitorFromWindow(trackedHwnd, MONITOR_DEFAULTTONEAREST)
			: nullptr;
		if (hmon == cachedMon) return;

		closeAdapter();
		refreshHz = 0.0;
		cachedMon = hmon;
		if (!hmon) return;

		openAdapterForMonitor(hmon);
		updateRefreshFromCCD(hmon);
	}

	void updateRefreshFromCCD(HMONITOR hmon) const {
		MONITORINFOEXW mi{};
		mi.cbSize = sizeof(mi);
		if (!GetMonitorInfoW(hmon, (MONITORINFO*)&mi)) return;

		UINT32 pathCount = 0, modeCount = 0;
		if (GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS,
				&pathCount, &modeCount) != ERROR_SUCCESS) return;

		std::vector<DISPLAYCONFIG_PATH_INFO> paths(pathCount);
		std::vector<DISPLAYCONFIG_MODE_INFO> modes(modeCount);

		if (QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS,
				&pathCount, paths.data(),
				&modeCount, modes.data(),
				nullptr) != ERROR_SUCCESS) return;

		paths.resize(pathCount);

		for (const auto& path : paths) {
			DISPLAYCONFIG_SOURCE_DEVICE_NAME srcName{};
			srcName.header.type      = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
			srcName.header.size      = sizeof(srcName);
			srcName.header.adapterId = path.sourceInfo.adapterId;
			srcName.header.id        = path.sourceInfo.id;
			if (DisplayConfigGetDeviceInfo(&srcName.header) != ERROR_SUCCESS) continue;

			if (wcscmp(srcName.viewGdiDeviceName, mi.szDevice) != 0) continue;

			const DISPLAYCONFIG_RATIONAL& r = path.targetInfo.refreshRate;
			if (r.Denominator == 0) return;

			refreshHz = (double)r.Numerator / (double)r.Denominator;
			return;
		}
	}

	void openAdapterForMonitor(HMONITOR hmon) const {
		MONITORINFOEXA mi{};
		mi.cbSize = sizeof(mi);
		if (!GetMonitorInfoA(hmon, &mi)) return;

		HDC hdc = CreateDCA(mi.szDevice, mi.szDevice, nullptr, nullptr);
		if (!hdc) return;

		D3DKMT_OPENADAPTERFROMHDC oa{};
		oa.hDc = hdc;
		NTSTATUS st = D3DKMTOpenAdapterFromHdc(&oa);
		DeleteDC(hdc);
		if (st != 0) return;

		adapterHandle = oa.hAdapter;
		vidPnSourceId = oa.VidPnSourceId;
	}

	void closeAdapter() const {
		if (adapterHandle) {
			D3DKMT_CLOSEADAPTER ca{};
			ca.hAdapter = adapterHandle;
			D3DKMTCloseAdapter(&ca);
		}
		adapterHandle = 0;
		vidPnSourceId = 0;
	}

	HWND trackedHwnd = nullptr;

	mutable HMONITOR cachedMon     = nullptr;
	mutable UINT64   adapterHandle = 0;
	mutable UINT     vidPnSourceId = 0;
	mutable double   refreshHz     = 0.0;
};
