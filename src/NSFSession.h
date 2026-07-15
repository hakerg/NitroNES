#pragma once
#include "AppEvent.h"
#include "AppSettings.h"
#include "IFileSession.h"
#include "IWindow.h"
#include "core/NSFPlayer.h"

#include <sstream>
#include <iomanip>
#include <cmath>

static std::string toHex(uint16_t v) {
    std::ostringstream os;
    os << std::uppercase << std::hex << v;
    return os.str();
}

class NSFSession : public IFileSession, public NSFPlayer {
public:
    NSFSession(const std::string &path, IWindow &window,
               AppAudioStream &audio, AppSettings &settings)
        : IFileSession(path, audio, window, settings),
          NSFPlayer(settings.audioSettings, path) {}

    ~NSFSession() override = default;

    NESCoreBase& getCore() override { return *this; }

    std::string getInfo() override {
        const NSFHeader &h = header();
        std::string chips;
        if (h.extraChipFlags & 0x01) chips += "VRC6 ";
        if (h.extraChipFlags & 0x02) chips += "VRC7 ";
        if (h.extraChipFlags & 0x04) chips += "FDS ";
        if (h.extraChipFlags & 0x08) chips += "MMC5 ";
        if (h.extraChipFlags & 0x10) chips += "Namco 106 ";
        if (h.extraChipFlags & 0x20) chips += "Sunsoft FME-07 ";
        if (chips.empty()) chips = "none";
        std::string region = h.isDualMode() ? "Dual (NTSC/PAL)"
            : h.isPAL() ? "PAL" : "NTSC";
        std::string s;
        s += "File: " + filename + "\n";
        s += "Path: " + path + "\n";
        s += "Type: NSF\n";
        s += "Title: " + h.title() + "\n";
        s += "Artist: " + h.artistName() + "\n";
        s += "Copyright: " + h.copyrightText() + "\n";
        s += "Songs: " + std::to_string(h.totalSongs) + "\n";
        s += "Load: $" + toHex(h.loadAddr)
            + "  Init: $" + toHex(h.initAddr)
            + "  Play: $" + toHex(h.playAddr) + "\n";
        s += "Region: " + region + "\n";
        s += "Bankswitch: " + std::string(h.isBankswitched() ? "yes" : "no") + "\n";
        s += "Chips: " + chips + "\n";
        double rate = getBaseFramerate();
        if (rate > 0.0)
            s += "Play rate: " + std::to_string((int)std::round(rate)) + " Hz\n";

        return s;
    }

    void processKeyDown(AppKey key) override {
        switch (key) {
        case AppKey::NsfTogglePause:
            paused = !paused;
            break;
        case AppKey::NsfNextSong:
            nextSong();
            break;
        case AppKey::NsfPrevSong:
            prevSong();
            break;
        default:
            break;
        }
    }

protected:
    void pushAudioSample(float sample, double dt) override {
        audio.addNESSample(sample, dt);
    }
};