#pragma once
#include "ILanguage.h"
#include <string>
#include <unordered_map>

class Polish : public ILanguage {
public:
    const char *getName() const override { return "Polski"; }
    const char *getCode() const override { return "pl"; }

    const char *tr(const char *id) const override {
        static const std::unordered_map<std::string, const char *> dict = {
            {"file", "Plik"},
            {"file.open", "Otwórz..."},
            {"file.reload", "Przeładuj"},
            {"file.close", "Zamknij"},
            {"file.quit", "Wyjdź"},
            {"emulation", "Emulacja"},
            {"emulation.system", "Podsystem"},
            {"emulation.pause", "Pauza"},
            {"emulation.reset", "Reset"},
            {"settings", "Ustawienia"},
            {"settings.language", "Język"},
            {"settings.sync", "Synchronizacja..."},
            {"settings.vsync", "Synchronizacja pionowa"},
            {"settings.sync_mode", "Tryb synchronizacji"},
            {"settings.sync.none", "Standardowy"},
            {"settings.sync.timer", "Częstotliwość odświeżania"},
            {"settings.sync.scanline", "Beam racing"},
            {"settings.current_speed", "Aktualna szybkość"},
            {"settings.scanline.buffer", "Bufor [ms]"},
            {"status.system_error", "Niedostępne: błąd systemu"},
            {"status.outside_tolerance", "Nieobsługiwane odświeżanie"},
            {"status.no_fullscreen", "Wymaga pełnego ekranu"},
            {"settings.audio", "Dźwięk..."},
            {"settings.volume", "Głośność"},
            {"settings.audio.filters", "Filtry"},
            {"settings.audio.hp90", "Filtr górnoprzepustowy 90 Hz"},
            {"settings.audio.hp440", "Filtr górnoprzepustowy 440 Hz"},
            {"settings.audio.lp14k", "Filtr dolnoprzepustowy 14 kHz"},
            {"settings.audio.reduce_clicks", "Zredukuj artefakty"},
            {"settings.audio.adjust_pitch", "Dostosuj wysokość dźwięku do prędkości"},
            {"settings.controls", "Sterowanie..."},
            {"controls.title", "Sterowanie"},
            {"controls.pad1", "Pad 1"},
            {"controls.pad2", "Pad 2"},
            {"controls.emulation", "Emulacja"},
            {"controls.nsf", "NSF"},
            {"controls.up", "Góra"},
            {"controls.down", "Dół"},
            {"controls.left", "Lewo"},
            {"controls.right", "Prawo"},
            {"controls.pause", "Pauza"},
            {"controls.fullscreen", "Pełny ekran"},
            {"controls.speedup", "Przyspieszenie"},
            {"controls.speeddown", "Spowolnienie"},
            {"controls.reset", "Reset"},
            {"controls.nsf.pause", "Pauza"},
            {"controls.nsf.next", "Następny utwór"},
            {"controls.nsf.prev", "Poprzedni utwór"},
            {"controls.bind_all", "Ustaw wszystkie"},
            {"controls.clear", "Wyczyść"},
            {"controls.clear_all", "Wyczyść wszystkie"},
            {"controls.action", "Akcja"},
            {"controls.key", "Klawisz"},
            {"controls.press_key", "Naciśnij klawisz..."},
            {"controls.set", "Ustaw"},
            {"controls.cancel", "Anuluj"}};
        auto it = dict.find(id);
        return it != dict.end() ? it->second : id;
    }
};