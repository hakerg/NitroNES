#pragma once
#include "ILanguage.h"
#include <unordered_map>
#include <string>

class Polish : public ILanguage {
public:
	const char* getName() const override { return "Polski"; }

	const char* tr(const char* id) const override {
		static const std::unordered_map<std::string, const char*> dict = {
			{ "file",                        "Plik"                                         },
			{ "file.open",                   "Otwórz..."                                    },
			{ "file.reload",                 "Przeładuj"                                    },
			{ "file.quit",                   "Wyjdź"                                        },
			{ "emulation",                   "Emulacja"                                     },
			{ "emulation.system",            "Podsystem"                                    },
			{ "emulation.pause",             "Pauza"                                        },
			{ "emulation.reset",             "Reset"                                        },
			{ "settings",                    "Ustawienia"                                   },
			{ "settings.language",           "Język"                                        },
			{ "settings.sync",               "Synchronizacja"                               },
			{ "settings.vsync",              "Synchronizacja pionowa"                       },
			{ "settings.match_hz",           "Dopasuj szybkość do częstotliwości monitora"  },
			{ "settings.scanline",           "Dopasowanie linii skanowania"                 },
			{ "settings.scanline.enabled",   "Włączone"                                     },
			{ "settings.scanline.buffer",    "Bufor [ms]"                                   },
			{ "settings.audio",              "Dźwięk"                                       },
			{ "settings.volume",             "Głośność"                                     },
			{ "settings.audio.filters",      "Filtry"                                       },
			{ "settings.audio.hp90",         "Filtr górnoprzepustowy 90 Hz"                 },
			{ "settings.audio.hp440",        "Filtr górnoprzepustowy 440 Hz"                },
			{ "settings.audio.lp14k",        "Filtr dolnoprzepustowy 14 kHz"                },
			{ "settings.controls",           "Sterowanie..."                                },
		};
		auto it = dict.find(id);
		return it != dict.end() ? it->second : id;
	}
};
