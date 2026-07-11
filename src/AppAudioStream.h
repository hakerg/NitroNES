#pragma once
#include "core/AudioStream.h"

class AppAudioStream : public AudioStream {
public:
    AppAudioStream(AudioSettings &settings) : AudioStream(settings) {}
    ~AppAudioStream() override = default;

    virtual int getQueuedMs() = 0;
};
