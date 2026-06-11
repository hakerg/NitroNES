#pragma once
#include "AppSettings.h"
#include "IFileSession.h"
#include "IWindow.h"
#include "IInputContext.h"
#include "core/NESSystem.h"
#include "core/NESConst.h"
#include "core/NESCoordUtils.h"
#include <chrono>
#include <cmath>
#include <stdexcept>

class NESSession : public IFileSession, public INESSystemHost {
public:
	NESSession(
		const std::string& path,
		IInputContext& input,
		IWindow& window,
		AppSettings& settings,
		IEmulatorHost& host,
		AppAudioStream& audio)
		: IFileSession(path, audio, window, settings)
		, nes(host, *this)
		, input(input)
	{
		if (!nes.loadFile(path))
			throw std::runtime_error("[NESSession] Nie udalo sie zaladowac: " + path);
	}

	~NESSession() override { nes.shutdown(); }

	NESCoreBase& core() const override { return nes; }

	void runFrame(double baseSpeed) override {
		if (canUseScanlineSync(baseSpeed))
			runScanlineSync(baseSpeed);
		else
			runTimerSync(baseSpeed);

		nes.renderFrame();
	}

protected:
	void renderFrame(const uint32_t* frameBuffer) override {
		window.presentNESFrame(frameBuffer, *this);
	}

	uint8_t readController(int port) override {
		return input.readController(port);
	}

private:
	void runTimerSync(double baseSpeed) {
		auto now = std::chrono::steady_clock::now();
		double elapsed = std::chrono::duration<double>(now - timerPrev).count();
		timerPrev = now;
		timerLag += elapsed;
		if (timerLag > MAX_LAG) timerLag = MAX_LAG;

		if (nes.paused) {
			nes.renderFrame();
			return;
		}

		std::lock_guard<std::mutex> lock(coreMutex);
		updateSpeed(baseSpeed);
		while (timerLag > 0.0) {
			double dt;
			nes.tickFrame(dt);
			timerLag -= dt;
		}
	}

	void runScanlineSync(double baseSpeed) {
		int monitorScanline = 0;
		if (!window.getScanLine(monitorScanline)) { runTimerSync(baseSpeed); return; }

		MonitorGeometry geo = window.getMonitorGeometry();
		if (geo.width <= 0 || geo.height <= 0) { runTimerSync(baseSpeed); return; }

		double monitorHz = window.getRefreshHz();
		if (monitorHz <= 0.0) { runTimerSync(baseSpeed); return; }

		int winW = 0, winH = 0;
		window.getPixelSize(winW, winH);

		double delta = (settings.scanlineBufferMs / 1000.0) * monitorHz * geo.height;
		double predicted = monitorScanline + delta;

		float nesX = 0.0f, nesY = 0.0f;
		NES::monitorToNES(geo.x, geo.y, winW, winH, 0.0f, (float)predicted, nesX, nesY);

		int target = static_cast<int>(std::round(nesY)) % NES::TOTAL_SCANLINES;
		if (target < 0) target += NES::TOTAL_SCANLINES;

		std::lock_guard<std::mutex> lock(coreMutex);
		updateSpeed(baseSpeed);
		nes.tickWhile([&]() { return nes.getCurrentScanline() != target; });
	}

	static constexpr double MAX_LAG = 0.02;

	mutable NESSystem nes;
	IInputContext& input;

	std::chrono::steady_clock::time_point timerPrev = std::chrono::steady_clock::now();
	double timerLag = 0.0;
};