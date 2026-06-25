#pragma once

struct AudioSettings {
    float volume = 2.0f;
    bool useFilter90 = false;
    bool useFilter440 = false;
    bool useFilter14k = false;
    bool reduceClicks = false;
    float pitch = 1.0f;
};