#pragma once
#include "AppSettings.h"
#include "IFileSession.h"
#include "IInputContext.h"
#include "IWindow.h"
#include "core/NESSystem.h"

class NESSession : public IFileSession, public NESSystem {
public:
    NESSession(const std::string &path, IInputContext &input, IWindow &window,
               AppSettings &settings, AppAudioStream &audio)
        : IFileSession(path, audio, window, settings),
          NESSystem(settings.audioSettings, path),
          input(input) {
        NESBus::instance().useBackdropForBackground = settings.useBackdropForBackground;
        NESBus::instance().preserveAspectRatio = settings.preserveAspectRatio;
        setSystem(static_cast<NESStandard>(settings.system));
        NESSystem::reset();
    }

    ~NESSession() override = default;

    std::unique_ptr<NESSession> clone() const {
        auto copy = std::make_unique<NESSession>(path, input, window, settings, audio);
        copy->copyStateFrom(*this);
        return copy;
    }

    void restoreState(const NESSession& src) {
        copyStateFrom(src);
    }

    NESCoreBase& getCore() override { return *this; }

    std::string getInfo() override {
        const Cartridge &c = getCartridge();
        const char *mirror = nullptr;
        switch (c.getMirroring()) {
        case Mirroring::HORIZONTAL:  mirror = "Horizontal";  break;
        case Mirroring::VERTICAL:    mirror = "Vertical";    break;
        case Mirroring::ONESCREEN_LO: mirror = "One-screen (low)";  break;
        case Mirroring::ONESCREEN_HI: mirror = "One-screen (high)"; break;
        case Mirroring::FOURSCREEN:  mirror = "Four-screen"; break;
        }
        std::string s;
        s += "File: " + filename + "\n";
        s += "Path: " + path + "\n";
        s += "Mapper: #" + std::to_string(c.getMapperID()) + " ("
            + c.getMapper().name() + ")\n";
        s += "PRG ROM: " + std::to_string(c.getPrgBanks() * 16) + " KiB ("
            + std::to_string(c.getPrgBanks()) + " banks)\n";
        s += "CHR ROM: " + std::to_string(c.getChrBanks() * 8) + " KiB ("
            + std::to_string(c.getChrBanks()) + " banks)\n";
        s += "Mirroring: " + std::string(mirror) + "\n";
        s += "PRG RAM: " + std::string(c.hasPrgRam() ? "yes" : "no") + "\n";
        s += "Bus conflicts: " + std::string(c.hasBusConflicts() ? "yes" : "no") + "\n";
        s += "System: " + std::string(system == NESStandard::PAL ? "PAL"
                                    : system == NESStandard::DENDY ? "Dendy" : "NTSC") + "\n";
        return s;
    }

protected:
    void onFrameCompleted() override {
        input.tickFrame();
    }

    uint8_t readController(int port) override {
        return input.readController(port);
    }

    void pushAudioSample(float sample, double dt) override {
        audio.addNESSample(sample, dt);
    }

private:
    IInputContext &input;
};
