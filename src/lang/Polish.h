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
            {"settings.speed", "Bazowa prędkość"},
            {"settings.speed1", "Przyspieszenie"},
            {"settings.speed2", "Spowolnienie"},
            {"settings.sync_mode", "Dostosuj prędkość emulacji"},
            {"settings.sync.none", "Standardowa"},
            {"settings.sync.none.tooltip", "Uruchamia emulację z oryginalną, natywną prędkością konsoli."},
            {"settings.sync.refresh_rate", "Częstotliwość monitora"},
            {"settings.sync.refresh_rate.tooltip", "Wymaga monitora o odświeżaniu bliskim 60Hz (NTSC) / 50Hz (PAL) lub ich wielokrotności."},
            {"settings.sync.scanline", "Minimalne opóźnienie obrazu"},
            {"settings.sync.scanline.tooltip", "Synchronizuje emulację z wiązką monitora, minimalizując opóźnienia.\nWymaga pełnego ekranu i kompatybilnej częstotliwości odświeżania.\nAutomatycznie wyłącza VSync."},
            {"settings.current_speed", "Aktualna prędkość"},
            {"settings.scanline.buffer", "Bufor [ms]"},
            {"settings.scanline.buffer.tooltip", "Bufor dla trybu 'Minimalne opóźnienie obrazu'.\nUWAGA: Przy zbyt niskiej wartości (np. 1ms) emulator będzie spóźniać się o całą klatkę.\nTo ZWIĘKSZA opóźnienie, choć obraz wygląda idealnie płynnie.\nJak ustawić: obniżaj wartość, aż zobaczysz rwanie obrazu, a następnie lekko ją podnieś, aż rwanie ustąpi."},
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
            {"controls.emulation", "Ogólne"},
            {"controls.nsf", "Odtwarzacz NSF"},
            {"controls.up", "Góra"},
            {"controls.down", "Dół"},
            {"controls.left", "Lewo"},
            {"controls.right", "Prawo"},
            {"controls.pause", "Pauza"},
            {"controls.fullscreen", "Pełny ekran"},
            {"controls.speedup", "Przyspieszenie"},
            {"controls.speeddown", "Spowolnienie"},
            {"controls.reset", "Reset"},
            {"controls.open", "Otwórz plik"},
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