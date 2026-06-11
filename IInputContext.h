#pragma once
#include <cstdint>

class IInputContext {
public:
	virtual ~IInputContext() = default;

	// Odczyt stanu kontrolera NES (port 0 lub 1)
	virtual uint8_t readController(int port) const = 0;

	// Wywołanie na koniec każdej klatki (turbo tick)
	virtual void tickFrame() = 0;

	// Gamepad hotplug
	virtual void onGamepadAdded(uint32_t deviceId) = 0;
	virtual void onGamepadRemoved(uint32_t deviceId) = 0;

	// Trigger analogowy (szybkość)
	virtual void onRightTrigger(bool pressed) = 0;
	virtual void onLeftTrigger(bool pressed) = 0;
};
