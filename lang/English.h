#pragma once
#include "ILanguage.h"
#include <unordered_map>
#include <string>

class English : public ILanguage {
public:
	const char* getName() const override { return "English"; }

	const char* tr(const char* id) const override {
		static const std::unordered_map<std::string, const char*> dict = {
			{ "file",                        "File"                         },
			{ "file.open",                   "Open..."                      },
			{ "file.reload",                 "Reload"                       },
			{ "file.quit",                   "Quit"                         },
			{ "emulation",                   "Emulation"                    },
			{ "emulation.system",            "Subsystem"                    },
			{ "emulation.pause",             "Pause"                        },
			{ "emulation.reset",             "Reset"                        },
			{ "settings",                    "Settings"                     },
			{ "settings.language",           "Language"                     },
			{ "settings.sync",               "Synchronization"              },
			{ "settings.vsync",              "Vertical sync"                },
			{ "settings.match_hz",           "Match monitor refresh rate"   },
			{ "settings.scanline",           "Scanline sync"                },
			{ "settings.scanline.enabled",   "Enabled"                      },
			{ "settings.scanline.buffer",    "Buffer [ms]"                  },
			{ "settings.audio",              "Audio"                        },
			{ "settings.volume",             "Volume"                       },
			{ "settings.audio.filters",      "Filters"                      },
			{ "settings.audio.hp90",         "High-pass filter 90 Hz"       },
			{ "settings.audio.hp440",        "High-pass filter 440 Hz"      },
			{ "settings.audio.lp14k",        "Low-pass filter 14 kHz"       },
			{ "settings.controls",           "Controls..."                  },
		};
		auto it = dict.find(id);
		return it != dict.end() ? it->second : id;
	}
};
