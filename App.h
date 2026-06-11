#pragma once
#include "AppAudioStream.h"
#include "AppEvent.h"
#include "AppSettings.h"
#include "IFileSession.h"
#include "IWindow.h"
#include "IInputContext.h"
#include "IMenuHandler.h"
#include "NESSession.h"
#include "NSFSession.h"
#include <string>
#include <memory>

class App : public IEmulatorHost, public IMenuHandler {
public:
	App(const std::string& romPath,
		IWindow& window,
		IInputContext& input,
		AppAudioStream& audio,
		AppSettings& settings)
		: window(window)
		, input(input)
		, audio(audio)
		, settings(settings)
		, session(makeSession(romPath))
	{
		window.initMenu(settings, *this);
	}

	void run() {
		while (running) {
			AppEvent ev;
			while (window.pollEvent(ev))
				processEvent(ev);

			session->runFrame(calcSpeed());

			if (guiActive && !window.isMenuOpen() && window.getTicks() - lastMouseMoveTime >= 1000) {
				window.showCursor(false);
				guiActive = false;
			}

			window.delay(1);
		}
	}

protected:
	void pushAudioSample(float sample, double dt) override {
		audio.addNESSample(sample, dt);
	}

	void onFrameReady() override {
		input.tickFrame();
	}

	void onOpen() override {
		// TODO
	}

	void onReload() override {
		const std::string p = session->path;
		session.reset();
		session = makeSession(p);
	}

	void onQuit() override {
		running = false;
	}

	bool isVisible() override {
		return guiActive;
	}

private:
	std::unique_ptr<IFileSession> makeSession(const std::string& path) {
		if (IFileSession::isNesRomFile(path)) {
			return std::make_unique<NESSession>(
				path,
				input,
				window,
				settings,
				*this,
				audio);
		}
		return std::make_unique<NSFSession>(path, window, *this, audio, settings);
	}

	void processEvent(const AppEvent& ev) {
		switch (ev.type) {
		case AppEventType::Quit:
			running = false;
			return;
		case AppEventType::WindowResized:
			session->core().renderFrame();
			return;
		case AppEventType::MouseMoved:
			lastMouseMoveTime = window.getTicks();
			if (!guiActive) { window.showCursor(true); guiActive = true; }
			return;
		case AppEventType::KeyDown:
			handleKey(ev.key);
			return;
		case AppEventType::KeyUp:
			if (ev.key == AppKey::SpeedUp || ev.key == AppKey::SpeedDown) {
				keyFast = false; keySlow = false;
			}
			return;
		case AppEventType::GamepadAxisRightTrigger:
			padFast = ev.axisDown;
			if (!padFast && !padSlow) speedPadId = 0;
			else                      speedPadId = ev.deviceId;
			return;
		case AppEventType::GamepadAxisLeftTrigger:
			padSlow = ev.axisDown;
			if (!padFast && !padSlow) speedPadId = 0;
			else                      speedPadId = ev.deviceId;
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
			session->core().paused = !session->core().paused;
			return;
		case AppKey::FullScreen:
			window.toggleFullscreen();
			return;
		case AppKey::SpeedUp:
			keyFast = true; keySlow = false;
			return;
		case AppKey::SpeedDown:
			keySlow = true; keyFast = false;
			return;
		default:
			session->processKeyDown(key);
			return;
		}
	}

	double calcSpeed() const {
		if (keyFast || padFast) return 4.0;
		else if (keySlow || padSlow) return 0.5;
		else                         return 1.0;
	}

	IWindow& window;
	IInputContext& input;
	AppAudioStream& audio;
	AppSettings& settings;

	std::unique_ptr<IFileSession> session;

	bool     running = true;
	bool     guiActive = true;
	bool     keyFast = false;
	bool     keySlow = false;
	bool     padFast = false;
	bool     padSlow = false;
	uint32_t speedPadId = 0;
	uint64_t lastMouseMoveTime = 0;
};