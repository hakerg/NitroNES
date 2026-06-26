#pragma once
#include "AppEvent.h"
#include "AppSettings.h"
#include "IFileSession.h"
#include "IWindow.h"
#include "core/NSFPlayer.h"

class NSFSession : public IFileSession {
public:
    NSFSession(const std::string &path, IWindow &window, IEmulatorHost &host,
               AppAudioStream &audio, AppSettings &settings)
        : IFileSession(path, audio, window, settings),
          nsf(host, settings.audioSettings, path) {}

    ~NSFSession() override {
    }

    NESCoreBase& getCore() const override { return nsf; }

    void processKeyDown(AppKey key) override {
        switch (key) {
        case AppKey::NsfTogglePause:
            nsf.paused = !nsf.paused;
            break;
        case AppKey::NsfNextSong:
            nsf.nextSong();
            break;
        case AppKey::NsfPrevSong:
            nsf.prevSong();
            break;
        default:
            break;
        }
    }

private:
    mutable NSFPlayer nsf;
};