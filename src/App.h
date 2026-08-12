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
#include <iostream>
#include <string>

class App : public IMenuHandler {
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
            window.presentNESFrame(session.get());

            applyPendingSessionAction();

            if (guiActive && !window.isMenuOpen() && session &&
                window.getTicks() - lastMouseMoveTime >= 1000) {
                window.showCursor(false);
                guiActive = false;
            }
        }
    }

    void pushAudioSample(float sample, double dt) {
        audio.addNESSample(sample, dt);
    }


    void onOpen()   override { pending = PendingAction::Open;   }
    void onReload() override { pending = PendingAction::Reload; }
    void onClose()  override { pending = PendingAction::Close;  }

    void onReset() override {
        if (!session)
            return;
        session->getCore().reset();
    }

    void onSaveState(int slot) override {
        if (session) session->saveStateToFile(slot);
    }

    void onLoadState(int slot) override {
        if (session) session->loadStateFromFile(slot);
    }

    void onQuit() override { running = false; }

    bool isMenuVisible() override { return guiActive; }

private:
    enum class PendingAction { None, Open, Reload, Close };

    void applyPendingSessionAction() {
        const PendingAction action = pending;
        pending = PendingAction::None;
        switch (action) {
            case PendingAction::None:
                return;
            case PendingAction::Open: {
                std::string newPath = window.openFileDialog();
                if (newPath.empty()) return;
                loadFile(newPath);
                return;
            }
            case PendingAction::Reload: {
                if (!session) return;
                const std::string p = session->path;
                session.reset();
                try {
                    session = makeSession(p);
                } catch (const std::exception& e) {
                    std::cerr << "[App] failed to reload " << p << ": " << e.what() << "\n";
                }
                return;
            }
            case PendingAction::Close:
                session.reset();
                return;
        }
    }

    std::unique_ptr<IFileSession> makeSession(const std::string &path) {
        if (IFileSession::isNesRomFile(path)) {
            return std::make_unique<NESSession>(path, input, window, settings,
                                                audio);
        }
        return std::make_unique<NSFSession>(path, window, audio,
                                            settings);
    }

    void loadFile(const std::string &path) {
        session.reset();
        try {
            session = makeSession(path);
        } catch (const std::exception &e) {
            std::cerr << "[App] failed to open " << path << ": " << e.what() << "\n";
        }
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
        case AppEventType::DropFile:
            if (!ev.dropPath.empty())
                loadFile(ev.dropPath);
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
        case AppKey::Open:
            onOpen();
            return;
        case AppKey::Reload:
            onReload();
            return;
        default: {
            int v = static_cast<int>(key);
            int saveBase = static_cast<int>(AppKey::SaveState0);
            if (v >= saveBase && v < saveBase + 10) {
                onSaveState(v - saveBase);
                return;
            }
            int loadBase = static_cast<int>(AppKey::LoadState0);
            if (v >= loadBase && v < loadBase + 10) {
                onLoadState(v - loadBase);
                return;
            }
            if (session)
                session->processKeyDown(key);
            return;
        }
        }
    }

    double calcSpeed() const {
        double s = settings.speed;
        if (settings.keys.speedUp.active() || padFast)
            s = settings.speed1;
        else if (settings.keys.speedDown.active() || padSlow)
            s = settings.speed2;
        if (settings.keys.rewind.active())
            return -s;
        return s;
    }

    IWindow &window;
    IInputContext &input;
    AppAudioStream &audio;
    AppSettings &settings;

    std::unique_ptr<IFileSession> session;

    PendingAction pending = PendingAction::None;

    bool running = true;
    bool guiActive = true;
    bool padFast = false;
    bool padSlow = false;
    uint32_t speedPadId = 0;
    uint64_t lastMouseMoveTime = 0;
};