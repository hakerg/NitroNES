#pragma once
#include "../src/core/NSFPlayer.h"

class NESHeadlessNSF : public NSFPlayer {
public:
    explicit NESHeadlessNSF(const std::string& path)
        : NSFPlayer(audioSettings, path) {
        initSong(getCurrentSong());
    }

    void setController1(uint8_t buttons) { controller1 = buttons; }
    void setController2(uint8_t buttons) { controller2 = buttons; }
    uint8_t getController1() const { return controller1; }
    uint8_t getController2() const { return controller2; }

    void tickCycles(uint64_t n) {
        const uint64_t target = cycleNo() + n;
        tickWhile([&]{ return cycleNo() < target; });
    }

    uint64_t frameNo() { return (uint64_t)getCompletedFramesCount(); }
    uint64_t cycleNo() { return a2a03.getCPU().getCycle(); }

protected:
    void pushAudioSample(float sample, double dt) override {}

private:
    AudioSettings audioSettings;
    uint8_t controller1 = 0x00;
    uint8_t controller2 = 0x00;
};


