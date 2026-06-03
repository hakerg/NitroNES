#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <timeapi.h>

class PrecisionSleeper {
public:
	PrecisionSleeper() {
		hTimer = CreateWaitableTimerExW(nullptr, nullptr,
			CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
	}

	~PrecisionSleeper() {
		if (hTimer) { CloseHandle(hTimer); hTimer = nullptr; }
	}

	// Brak kopii
	PrecisionSleeper(const PrecisionSleeper&)            = delete;
	PrecisionSleeper& operator=(const PrecisionSleeper&) = delete;

	bool isHighResAvailable() const { return hTimer != nullptr; }

	void sleep(double seconds) {
		if (hTimer) {
			LARGE_INTEGER due;
			due.QuadPart = -(LONGLONG)(seconds * 1e7);
			SetWaitableTimerEx(hTimer, &due, 0, nullptr, nullptr, nullptr, 0);
			WaitForSingleObject(hTimer, INFINITE);
		}
	}

private:
	HANDLE hTimer = nullptr;
};
