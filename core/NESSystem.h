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
    virtual void renderFrame(const uint32_t* frameBuffer) = 0;
    virtual uint8_t readController(int port) = 0;
};

class NESSystem : public NESCoreBase {
public:
    explicit NESSystem(IEmulatorHost& host, INESSystemHost& nesHost, const std::string& path)
        : NESCoreBase(host), ppu(*this), nesHost(nesHost) {
        cart = std::make_unique<Cartridge>(path);
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

    void renderFrame() override {
        nesHost.renderFrame(ppu.getFramebuffer());
    }

    const PPU2C02* getPPU() const override { return &ppu; }

    bool pollNMI() override {
        return ppu.nmiLineLow();
    }

    bool pollIRQ() override {
        return a2a03.getAPU().irqAsserted() || (cart && cart->irqState());
    }

protected:
    uint8_t memRead(uint16_t addr) override {
        uint8_t data = a2a03.getDataBus();

        if (addr < 0x2000) {
            data = cpuRam[addr & 0x07FF];
        }
        else if (addr < 0x4000) {
            data = ppu.cpuRead(addr);
        }
        else if (addr == 0x4016) {
            uint8_t bit = (controllerShift & 0x80) ? 1 : 0;
            if (!controllerStrobe) controllerShift = (controllerShift << 1) | 0x01;
            data = (data & 0xE0) | bit;
        }
        else if (addr == 0x4017) {
            uint8_t bit = (controllerShift2 & 0x80) ? 1 : 0;
            if (!controllerStrobe) controllerShift2 = (controllerShift2 << 1) | 0x01;
            data = (data & 0xE0) | bit;
        }
        else if (addr >= 0x4020) {
            data = cart ? cart->cpuRead(addr, data) : data;
        }

        return data;
    }

    void memWrite(uint16_t addr, uint8_t data) override {
        if (addr < 0x2000) { cpuRam[addr & 0x07FF] = data; return; }
        if (addr < 0x4000) { ppu.cpuWrite(addr, data); return; }
        if (addr == 0x4016) {
            controllerStrobe = (data & 0x01) != 0;
            return;
        }
        if (cart && addr >= 0x4020) cart->cpuWrite(addr, data);
    }

    void  clockMapper() override { if (cart) cart->clock(); }
    float mapperAudio() const override { return cart ? cart->audioOutput() : 0.0f; }
    bool  mapperIRQ() const override { return cart && cart->irqState(); }
    void  mapperIrqAck() override { if (cart) cart->irqClear(); }

    void onPreStep() override {
        if (controllerStrobe) {
            controllerShift  = nesHost.readController(0);
            controllerShift2 = nesHost.readController(1);
        }
    }

    PPU2C02 ppu;
    std::unique_ptr<Cartridge> cart;

    uint8_t controllerShift  = 0x00;
    uint8_t controllerShift2 = 0x00;
    bool    controllerStrobe = false;

private:
    INESSystemHost& nesHost;
};