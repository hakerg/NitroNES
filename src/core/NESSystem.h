#pragma once
#include <array>
#include <span>
#include <spanstream>
#include <vector>
#include "NESCoreBase.h"
#include "NESBus.h"
#include "Cartridge.h"
#include "PPU2C02.h"

class NESSystem : public NESCoreBase {
public:
    explicit NESSystem(AudioSettings& audioSettings, const std::string& path)
        : cart(path, audioSettings) {
        auto& bus = NESBus::instance();
        bus.cart = &cart;
        bus.ppu = &ppu;
        bus.apu = &a2a03.getAPU();
        bus.core = this;
        bus.cpuBus = &a2a03;
        bus.dmaBus = &a2a03;
        bus.audio = &audioSettings;
        a2a03.getCPU().A = 0; a2a03.getCPU().X = 0; a2a03.getCPU().Y = 0;
        a2a03.getCPU().S = 0x00;
        a2a03.getCPU().P = CPU6502::FLAG_U | CPU6502::FLAG_B;

        cart.reset();
        a2a03.reset();
        ppu.reset();
    }

    void reset() override {
        cart.reset();
        a2a03.reset();
        ppu.reset();
    }

    bool pollNMI() override { return !ppu.pollNmiLow(); }
    bool irqAsserted() override { return cart.irqState(); }
    int getCompletedFramesCount() override { return ppu.getCompletedFramesCount(); }
    uint32_t* getFramebuffer() override { return ppu.getFramebuffer(); }

    void setTracer(Tracer* t) override {
        NESCoreBase::setTracer(t);
        ppu.setTracer(t);
    }

    void latchControllers() override {
        controllerShift  = readController(0);
        controllerShift2 = readController(1);
    }

    void save(std::ostream& s) const {
        cart.save(s);
        auto* baseEnd = reinterpret_cast<const char*>(&cart);
        s.write(reinterpret_cast<const char*>(this), baseEnd - reinterpret_cast<const char*>(this));
        s.write(reinterpret_cast<const char*>(&ppu),
                reinterpret_cast<const char*>(&lastBusReadAddr + 1) -
                    reinterpret_cast<const char*>(&ppu));
    }

    void load(std::istream& s) {
        cart.load(s);
        auto* baseEnd = reinterpret_cast<char*>(&cart);
        s.read(reinterpret_cast<char*>(this), baseEnd - reinterpret_cast<char*>(this));
        s.read(reinterpret_cast<char*>(&ppu),
                reinterpret_cast<char*>(&lastBusReadAddr + 1) -
                    reinterpret_cast<char*>(&ppu));
    }

    size_t stateSize() const {
        return cart.saveSize() + sysBlockSize();
    }

    void saveToBuf(uint8_t* buf) const {
        std::ospanstream ss(std::span(reinterpret_cast<char*>(buf), stateSize()));
        save(ss);
    }

    void loadFromBuf(const uint8_t* buf) {
        std::ispanstream ss(std::span(reinterpret_cast<const char*>(buf), stateSize()));
        load(ss);
    }

    std::vector<uint8_t> saveState() const override {
        std::vector<uint8_t> buf(stateSize());
        saveToBuf(buf.data());
        return buf;
    }

    void loadState(const std::vector<uint8_t>& data) override {
        loadFromBuf(data.data());
    }

private:
    size_t sysBlockSize() const {
        auto* baseEnd = reinterpret_cast<const char*>(&cart);
        size_t baseSize = baseEnd - reinterpret_cast<const char*>(this);
        size_t ppuSize = reinterpret_cast<const char*>(&lastBusReadAddr + 1)
                       - reinterpret_cast<const char*>(&ppu);
        return baseSize + ppuSize;
    }

public:
    double getBaseFramerate() const override {
        return system == NESStandard::NTSC ? NES::REFRESH_RATE_NTSC_ON
                                           : NES::REFRESH_RATE_PAL;
    }

    const Cartridge& getCartridge() const { return cart; }

    uint8_t peekMemory(uint16_t addr) override {
        if (addr < 0x2000) return cpuRam[addr & 0x07FF];
        if (addr < 0x4000) return ppu.cpuPeek(addr);
        if (addr == 0x4015) return a2a03.getAPU().cpuPeek(addr);
        if (addr == 0x4016) return controllerShift  & 0x01;
        if (addr == 0x4017) return controllerShift2 & 0x01;
        if (addr >= 0x4020) return cart.cpuRead(addr, 0x00);
        return 0xFF;
    }

    int getCurrentScanline()  override { return ppu.getScanline(); }
    int getTotalScanlines()   override { return ppu.getTotalScanlines(); }

protected:
    void copyStateFrom(const NESSystem& src) {
        *this = src;
    }

    virtual uint8_t readController(int port) = 0;

    void applySystem() override {
        a2a03.setRegion(system);
        ppu.setRegion(system);
    }

    void clockOneCycle() override {
        ppu.clock();
        ppu.clock();
        cart.clock();
        a2a03.clockPhi1();
        a2a03.clockPhi2Write();
        ppu.clock();
        if (system == NESStandard::PAL && a2a03.getCPU().getCycle() % 5 == 0)
            ppu.clock();
        a2a03.clockPhi2();

        pushAudioOutput(cart.audioOutput());
    }

    uint8_t readMemory(uint16_t addr) override {
        uint8_t data = a2a03.getBusData();

        const uint16_t prevAddr = lastBusReadAddr;
        lastBusReadAddr = addr;

        if (addr < 0x2000) {
            data = cpuRam[addr & 0x07FF];
        } else if (addr < 0x4000) {
            data = ppu.cpuRead(addr);
        } else if (addr == 0x4016) {
            const bool newRun = (prevAddr != 0x4016);
            const bool strobe = a2a03.isControllerStrobeActive();

            if (newRun || strobe)
                controllerLatch = (controllerShift & 0x80) ? 1 : 0;
            if (newRun && !strobe)
                controllerShift = (controllerShift << 1) | 0x01;

            data = (data & 0xE0) | controllerLatch;
        } else if (addr == 0x4017) {
            const bool newRun = (prevAddr != 0x4017);
            const bool strobe = a2a03.isControllerStrobeActive();

            if (newRun || strobe)
                controllerLatch2 = (controllerShift2 & 0x80) ? 1 : 0;
            if (newRun && !strobe)
                controllerShift2 = (controllerShift2 << 1) | 0x01;

            data = (data & 0xE0) | controllerLatch2;
        } else if (addr >= 0x4020) {
            data = cart.cpuRead(addr, data);
        }

        return data;
    }

    uint8_t readMemoryExternal(uint16_t addr) override {
        const uint16_t saved = lastBusReadAddr;
        const uint8_t data = readMemory(addr);
        lastBusReadAddr = saved;
        return data;
    }

    void writeMemoryMapped(uint16_t addr, uint8_t data) override {
        if (addr < 0x2000)  { cpuRam[addr & 0x07FF] = data; return; }
        if (addr < 0x4000)  { ppu.cpuWrite(addr, data); return; }
        if (addr >= 0x4020) cart.cpuWrite(addr, data);
    }

    Cartridge cart;
    PPU2C02   ppu;

    uint8_t controllerShift  = 0x00;
    uint8_t controllerShift2 = 0x00;
    uint8_t controllerLatch  = 0x00;
    uint8_t controllerLatch2 = 0x00;
    uint16_t lastBusReadAddr = 0xFFFF;
};
