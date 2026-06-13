#pragma once
#include "AppEvent.h"
#include "AppSettings.h"
#include "IMenuHandler.h"
#include <cstdint>
#include <string>

class IFileSession;
struct AppSettings;

class IWindow {
public:
	virtual ~IWindow() = default;

	// --- Okno i Zdarzenia ---
	virtual bool pollEvent(AppEvent& out) = 0;
	virtual void showCursor(bool visible) = 0;
	virtual void toggleFullscreen() = 0;
	virtual void setTitle(const std::string& title) = 0;
	virtual uint64_t getTicks() const = 0;
	virtual void delay(uint32_t ms) = 0;
	virtual void getPixelSize(int& w, int& h) const = 0;

	// --- Renderowanie ---
	virtual void presentNESFrame(const uint32_t* framebuffer, IFileSession& session) = 0;
	virtual void presentBlank(IFileSession& session) = 0;

	// --- GUI (Menu) ---
	virtual void initMenu(AppSettings& settings, IMenuHandler& handler) = 0;
	virtual bool isMenuOpen() const = 0;

	// --- Sprzęt i Synchronizacja ---
	virtual bool getScanLine(int& outRaw) const = 0;
	virtual double getRefreshHz() const = 0;
	virtual void getMonitorGeometry(int& w, int& h) const = 0;
};
