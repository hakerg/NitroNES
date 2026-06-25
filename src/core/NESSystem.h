#pragma once
#include <array>
#include <memory>
#include <stdexcept>
#include "NESCoreBase.h"
#include "Cartridge.h"
#include "PPU2C02.h"

class INESSystemHost {
public:
    virtual ~INESSystemHost() = default;
    virtual uint8_t readController(int port) = 0;
};

class NESSystem : public NESCoreBase {
public:
    explicit NESSystem(IEmulatorHost& host, INESSystemHost& nesHost, AudioSettings& audioSettings, const std::string& path)
        : NESCoreBase(host, audioSettings), ppu(*this), nesHost(nesHost) {
        cart = std::make_unique<Cartridge>(path, audioSettings);
        if (!cart->isImageValid()) {
            cart.reset();
            throw std::runtime_error("[NES] Nie udalo sie zaladowac: " + path);
        }
        ppu.cart = cart.get();
        pal = false;
        a2a03.getAPU().setPAL(pal);
        cpuRam.fill(0x00);
        a2a03.getCPU().A = 0; a2a03.getCPU().X = 0; a2a03.getCPU().Y = 0;
        a2a03.getCPU().S = 0x00;
        a2a03.getCPU().P = CPU6502::FLAG_U | CPU6502::FLAG_B;
        reset();
    }

    void reset() override {
        if (cart) cart->reset();
        a2a03.reset();
        ppu.reset();
    }

    uint32_t* getFramebuffer() {
        return ppu.getFramebuffer();
    }

    PPU2C02* getPPU() override { return &ppu; }

    bool pollNMI() override { return !ppu.pollNmiLow(); }
    bool irqAsserted() override { return cart && cart->irqState(); }

protected:
    uint8_t memRead(uint16_t addr) override {
        uint8_t data = a2a03.getBusData();

        const uint16_t prevAddr = lastBusReadAddr;
        lastBusReadAddr = addr;

        if (addr < 0x2000) {
            data = cpuRam[addr & 0x07FF];
        } else if (addr < 0x4000) {
            data = ppu.cpuRead(addr);
        } else if (addr == 0x4016) {
            const bool newRun = (prevAddr != 0x4016);
            if (newRun || controllerStrobe)
                controllerLatch = (controllerShift & 0x80) ? 1 : 0;
            if (newRun && !controllerStrobe)
                controllerShift = (controllerShift << 1) | 0x01;
            data = (data & 0xE0) | controllerLatch;
        } else if (addr == 0x4017) {
            const bool newRun = (prevAddr != 0x4017);
            if (newRun || controllerStrobe)
                controllerLatch2 = (controllerShift2 & 0x80) ? 1 : 0;
            if (newRun && !controllerStrobe)
                controllerShift2 = (controllerShift2 << 1) | 0x01;
            data = (data & 0xE0) | controllerLatch2;
        } else if (addr >= 0x4020) {
            data = cart ? cart->cpuRead(addr, data) : data;
        }

        return data;
    }

    uint8_t memReadExternal(uint16_t addr) override {
        const uint16_t saved = lastBusReadAddr;
        const uint8_t data = memRead(addr);
        lastBusReadAddr = saved;
        return data;
    }

    void memWrite(uint16_t addr, uint8_t data) override {
        if (addr < 0x2000)  { cpuRam[addr & 0x07FF] = data; return; }
        if (addr < 0x4000)  { ppu.cpuWrite(addr, data); return; }
        if (addr == 0x4016) { controllerStrobe = (data & 0x01) != 0; return; }
        if (cart && addr >= 0x4020) cart->cpuWrite(addr, data);
    }

    void  clockMapper() override { if (cart) cart->clock(); }
    float mapperAudio() const override { return cart ? cart->audioOutput() : 0.0f; }

    void onPreStep() override {
    }

    void onPostStep() override {
        if (controllerStrobe && !a2a03.lastWasPutCycle()) {
            controllerShift  = nesHost.readController(0);
            controllerShift2 = nesHost.readController(1);
        }
    }

    PPU2C02 ppu;
    std::unique_ptr<Cartridge> cart;

    uint8_t controllerShift  = 0x00;
    uint8_t controllerShift2 = 0x00;
    uint8_t controllerLatch  = 0x00;
    uint8_t controllerLatch2 = 0x00;
    uint16_t lastBusReadAddr = 0xFFFF;
    bool    controllerStrobe = false;

private:
    INESSystemHost& nesHost;
};