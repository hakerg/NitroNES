#pragma once
#include "AppAudioStream.h"
#include "AppEvent.h"
#include "AppSettings.h"
#include "IFileSession.h"
#include "IInputContext.h"
#include "IMenuHandler.h"
#include "IWindow.h"
#include "NESSession.h"
#include "NSFSession.h"
#include <memory>
#include <string>

class App : public IEmulatorHost, public IMenuHandler {
public:
    App(const std::string &romPath, IWindow &window, IInputContext &input,
        AppAudioStream &audio, AppSettings &settings)
        : window(window), input(input), audio(audio), settings(settings),
          session(romPath.empty() ? nullptr : makeSession(romPath)) {
        window.initMenu(settings, *this, input);
    }

    void run() {
        while (running) {
            AppEvent ev;
            while (window.pollEvent(ev))
                processEvent(ev);

            double baseSpeed = calcSpeed();
            if (session) {
                session->clockCore(baseSpeed);
            }
            window.presentNESFrame(session.get(), baseSpeed);

            if (guiActive && !window.isMenuOpen() && session &&
                window.getTicks() - lastMouseMoveTime >= 1000) {
                window.showCursor(false);
                guiActive = false;
            }
        }
    }

    void pushAudioSample(float sample, double dt) override {
        audio.addNESSample(sample, dt);
    }

    void onFrameReady() override { input.tickFrame(); }

    void onOpen() override {
        std::string newPath = window.openFileDialog();
        if (newPath.empty())
            return;
        session.reset();
        session = makeSession(newPath);
    }

    void onReload() override {
        if (!session)
            return;
        const std::string p = session->path;
        session.reset();
        session = makeSession(p);
    }

    void onClose() override {
        session.reset();
    }

    void onReset() override {
        if (!session)
            return;
        session->getCore().reset();
    }

    void onQuit() override { running = false; }

    bool isMenuVisible() override { return guiActive; }

private:
    std::unique_ptr<IFileSession> makeSession(const std::string &path) {
        if (IFileSession::isNesRomFile(path)) {
            return std::make_unique<NESSession>(path, input, window, settings,
                                                *this, audio);
        }
        return std::make_unique<NSFSession>(path, window, *this, audio,
                                            settings);
    }

    void processEvent(const AppEvent &ev) {
        switch (ev.type) {
        case AppEventType::Quit:
            running = false;
            return;
        case AppEventType::WindowResized:
            return;
        case AppEventType::MouseMoved:
        case AppEventType::MouseButtonDown:
            lastMouseMoveTime = window.getTicks();
            if (!guiActive) {
                window.showCursor(true);
                guiActive = true;
            }
            return;
        case AppEventType::KeyDown:
            handleKey(ev.key);
            return;
        case AppEventType::KeyUp:
            return;
        case AppEventType::GamepadAxisRightTrigger:
            padFast = ev.axisDown;
            if (!padFast && !padSlow)
                speedPadId = 0;
            else
                speedPadId = ev.deviceId;
            return;
        case AppEventType::GamepadAxisLeftTrigger:
            padSlow = ev.axisDown;
            if (!padFast && !padSlow)
                speedPadId = 0;
            else
                speedPadId = ev.deviceId;
            return;
        case AppEventType::GamepadAdded:
            input.onGamepadAdded(ev.deviceId);
            return;
        case AppEventType::GamepadRemoved:
            input.onGamepadRemoved(ev.deviceId);
            return;
        default:
            break;
        }
    }

    void handleKey(AppKey key) {
        switch (key) {
        case AppKey::Pause:
            if (session)
                session->getCore().paused = !session->getCore().paused;
            return;
        case AppKey::FullScreen:
            window.toggleFullscreen();
            return;
        case AppKey::Reset:
            onReset();
            return;
        default:
            if (session)
                session->processKeyDown(key);
            return;
        }
    }

    double calcSpeed() const {
        if (settings.keys.speedUp.active() || padFast)
            return 4.0;
        if (settings.keys.speedDown.active() || padSlow)
            return 0.5;
        return 1.0;
    }

    IWindow &window;
    IInputContext &input;
    AppAudioStream &audio;
    AppSettings &settings;

    std::unique_ptr<IFileSession> session;

    bool running = true;
    bool guiActive = true;
    bool padFast = false;
    bool padSlow = false;
    uint32_t speedPadId = 0;
    uint64_t lastMouseMoveTime = 0;
};