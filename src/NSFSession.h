#pragma once
#include "AppEvent.h"
#include "AppSettings.h"
#include "IFileSession.h"
#include "IWindow.h"
#include "core/NSFPlayer.h"

class NSFSession : public IFileSession, public NSFPlayer {
public:
    NSFSession(const std::string &path, IWindow &window,
               AppAudioStream &audio, AppSettings &settings)
        : IFileSession(path, audio, window, settings),
          NSFPlayer(settings.audioSettings, path) {}

    ~NSFSession() override = default;

    NESCoreBase& getCore() override { return *this; }

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