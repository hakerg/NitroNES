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
            {"file.open", "Otwórz"},
            {"file.reload", "Przeładuj"},
            {"file.close", "Zamknij"},
            {"file.quit", "Wyjdź"},
            {"emulation", "Emulacja"},
            {"emulation.system", "System"},
            {"emulation.pause", "Pauza"},
            {"emulation.reset", "Reset"},
            {"settings", "Ustawienia"},
            {"tools", "Narzędzia"},
            {"tools.about_file", "O pliku"},
            {"tools.memory_viewer", "Przeglądarka pamięci"},
            {"tools.memory_viewer.address", "Adres"},
            {"settings.language", "Język"},
            {"settings.sync", "Synchronizacja"},
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
            {"settings.target_speed", "Docelowa prędkość"},
            {"settings.scanline.buffer", "Bufor [ms]"},
            {"settings.scanline.buffer.tooltip", "Bufor dla trybu 'Minimalne opóźnienie obrazu'.\nUWAGA: Przy zbyt niskiej wartości (np. 1ms) emulator będzie spóźniać się o całą klatkę.\nTo ZWIĘKSZA opóźnienie, choć obraz wygląda idealnie płynnie.\nJak ustawić: obniżaj wartość, aż zobaczysz rwanie obrazu, a następnie lekko ją podnieś, aż rwanie ustąpi."},
            {"settings.audio", "Dźwięk"},
            {"settings.volume", "Głośność"},
            {"settings.audio.use_filters", "Użyj oryginalnych filtrów"},
            {"settings.audio.adjust_pitch", "Dostosuj wysokość dźwięku do prędkości"},
            {"settings.controls", "Sterowanie"},
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
            {"controls.press_key", "Naciśnij klawisz"},
            {"controls.set", "Ustaw"},
            {"controls.cancel", "Anuluj"}};
        auto it = dict.find(id);
        return it != dict.end() ? it->second : id;
    }
};