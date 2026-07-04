#pragma once
#include "AppSettings.h"
#include "IFileSession.h"
#include "IInputContext.h"
#include "IWindow.h"
#include "core/NESSystem.h"

class NESSession : public IFileSession, public NESSystem {
public:
    NESSession(const std::string &path, IInputContext &input, IWindow &window,
               AppSettings &settings, AppAudioStream &audio)
        : IFileSession(path, audio, window, settings),
          NESSystem(settings.audioSettings, path),
          input(input) {}

    ~NESSession() override = default;

    NESCoreBase& getCore() override { return *this; }

protected:
    void onFrameCompleted() override {
        input.tickFrame();
    }

    uint8_t readController(int port) override {
        return input.readController(port);
    }

    void pushAudioSample(float sample, double dt) override {
        audio.addNESSample(sample, dt);
    }

private:
    IInputContext &input;
};