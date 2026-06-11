#pragma once
#include "core/AudioStream.h"

struct AppSettings {
	bool allowScanlineSync = false;
	bool vsync = false;
	bool matchRefreshRate = true;
	int scanlineBufferMs = 8;
	AudioSettings audioSettings;
};
