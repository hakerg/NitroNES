#pragma once
#include <mutex>
#include "core/AudioStream.h"

class IFileSession;

class AppAudioStream : public AudioStream {
public:
	AppAudioStream(AudioSettings& settings) : AudioStream(settings) {}
	virtual ~AppAudioStream() = default;

	virtual void attachSession(IFileSession& session) = 0;
	virtual void detachSession() = 0;
};
