#pragma once
#include <SDL3/SDL.h>
#include <functional>
#include <mutex>

class NESCoreBase;
class SDLAudioStream;

// Kontekst z zasobami przekazywany do strategii synchronizacji
struct SyncContext {
	// Zasoby
	NESCoreBase* core;
	SDLAudioStream* audioStream;
	SDL_Window* window;

	// Callbacki do obsługi eventów i akcji
	std::function<void(const SDL_Event&)> handleEvent;
	std::function<double()> getSpeed;

	std::mutex tickMutex;
};

class SyncStrategy {
public:
	explicit SyncStrategy(SyncContext& context) : ctx(&context) {}
	virtual ~SyncStrategy() = default;

	virtual void run() = 0;

protected:
	SyncContext* ctx = nullptr;
};
