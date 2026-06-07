#pragma once
#include "SyncStrategy.h"
#include "NESCoreBase.h"
#include <SDL3/SDL.h>
#include <mutex>

class TimerSyncStrategy : public SyncStrategy {
public:
    TimerSyncStrategy(SyncContext& context)
        : SyncStrategy(context)
        , freq(SDL_GetPerformanceFrequency())
        , prev(SDL_GetPerformanceCounter())
        , lag(0.0)
    {
    }

    void run() override {
        Uint64 now = SDL_GetPerformanceCounter();
        double elapsed = (double)(now - prev) / (double)freq;
        prev = now;
        lag += elapsed;
        if (lag > NES::MAX_LAG) lag = NES::MAX_LAG;

        bool shouldRender = false;

        if (ctx->getCore()->paused) {
            shouldRender = true;
        }
        else {
            ctx->updateSpeed(true);
            while (lag > 0.0) {
                double dt;
                ctx->getCore()->tickFrame(dt);
                lag -= dt;
                shouldRender = true;
            }
        }

        if (shouldRender) {
            ctx->getCore()->renderFrame();
        }
    }
private:
    Uint64 freq;
    Uint64 prev;
    double lag;
};