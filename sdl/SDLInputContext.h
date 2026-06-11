#pragma once
#include "../IInputContext.h"
#include "NESController.h"
#include "ControllerSettings.h"
#include <SDL3/SDL.h>

class SDLInputContext : public IInputContext {
public:
	SDLInputContext()
		: controller1(ControllerSettings::player1())
		, controller2(ControllerSettings::player2())
	{}

	uint8_t readController(int port) const override {
		return port == 0 ? controller1.readState() : controller2.readState();
	}

	void tickFrame() override {
		controller1.tickFrame();
		controller2.tickFrame();
	}

	void onGamepadAdded(uint32_t deviceId) override {
		if (!controller1.gamepad()) {
			if (SDL_Gamepad* gp = SDL_OpenGamepad(deviceId)) controller1.attachGamepad(gp);
		} else if (!controller2.gamepad()) {
			if (SDL_Gamepad* gp = SDL_OpenGamepad(deviceId)) controller2.attachGamepad(gp);
		}
	}

	void onGamepadRemoved(uint32_t deviceId) override {
		if (controller1.gamepadID() == deviceId) { controller1.closeAndDetachGamepad(); return; }
		if (controller2.gamepadID() == deviceId) { controller2.closeAndDetachGamepad(); }
	}

	void onRightTrigger(bool pressed) override { padFast = pressed; }
	void onLeftTrigger(bool pressed)  override { padSlow = pressed; }

	bool isFast() const { return padFast; }
	bool isSlow() const { return padSlow; }

private:
	NESController controller1;
	NESController controller2;
	bool padFast = false;
	bool padSlow = false;
};
