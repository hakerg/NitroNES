#pragma once
#include "core/AudioStream.h"

enum class AudioBufferHealth {
    Healthy,
    Underflow,
    Overflow
};

class AppAudioStream : public AudioStream {
public:
    AppAudioStream(AudioSettings &settings) : AudioStream(settings) {}
    virtual ~AppAudioStream() = default;

    virtual AudioBufferHealth getHealth() = 0;
};
