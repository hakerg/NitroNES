#pragma once
#include "IFileSession.h"
#include "IWindow.h"
#include "AppEvent.h"
#include "AppSettings.h"
#include "core/NSFPlayer.h"
#include <stdexcept>

class NSFSession : public IFileSession {
public:
	NSFSession(
		const std::string& path,
		IWindow& window,
		IEmulatorHost& host,
		AppAudioStream& audio,
		AppSettings& settings)
		: IFileSession(path, audio, window, settings)
		, nsf(host, path)
	{}

	~NSFSession() override { nsf.shutdown(); }

	NESCoreBase& core() const override { return nsf; }

	void processKeyDown(AppKey key) override {
		std::lock_guard lock(coreMutex);

		switch (key) {
		case AppKey::NsfTogglePause: nsf.paused = !nsf.paused; break;
		case AppKey::NsfNextSong:    nsf.nextSong();             break;
		case AppKey::NsfPrevSong:    nsf.prevSong();             break;
		default: break;
		}
	}

	void runFrame(double baseSpeed) override {
		updateSpeed(baseSpeed);
		window.presentBlank(*this);
	}

private:
	mutable NSFPlayer nsf;
};