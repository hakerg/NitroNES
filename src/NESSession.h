#pragma once
#include "AppSettings.h"
#include "IFileSession.h"
#include "IInputContext.h"
#include "IWindow.h"
#include "core/NESConst.h"
#include "core/NESSystem.h"

class NESSession : public IFileSession, public INESSystemHost {
public:
    NESSession(const std::string &path, IInputContext &input, IWindow &window,
               AppSettings &settings, IEmulatorHost &host,
               AppAudioStream &audio)
        : IFileSession(path, audio, window, settings),
          nes(host, *this, settings.audioSettings, path),
          input(input) {}

    ~NESSession() override {
    }

    NESCoreBase& getCore() const override { return nes; }

    uint8_t readController(int port) override {
        return input.readController(port);
    }

private:
    mutable NESSystem nes;
    IInputContext &input;
};