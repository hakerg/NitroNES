#pragma once
#include <cstdint>
#include <array>
#include <atomic>
#include <cstring>
#include <cmath>
#include <memory>
#include <iostream>
#include <functional>

#include "NESConst.h"
#include "NESCoreBase.h"
#include "Cartridge.h"
#include "PPUBus.h"
#include "PPU2C02.h"
#include "NESController.h"

// Rdzen NES: PPU, kontrolery, beam racer, render tekstury PPU.
class NESSystem : public NESCoreBase {
public:
    NESSystem() {
        ppu.ppuBus = &ppuBus;
        cpu.setIrqAckCallback([this]() { if (cart) cart->irqClear(); });

        controller2.key_A      = SDL_SCANCODE_Z;
        controller2.key_B      = SDL_SCANCODE_X;
        controller2.key_TurboA = SDL_SCANCODE_A;
        controller2.key_TurboB = SDL_SCANCODE_S;
        controller2.key_Select = SDL_SCANCODE_UNKNOWN;
        controller2.key_Start  = SDL_SCANCODE_UNKNOWN;
        controller2.key_Up     = SDL_SCANCODE_UP;
        controller2.key_Down   = SDL_SCANCODE_DOWN;
        controller2.key_Left   = SDL_SCANCODE_LEFT;
        controller2.key_Right  = SDL_SCANCODE_RIGHT;
        ppu.onFrameComplete = [this]() {
            controller.tickFrame();
            controller2.tickFrame();
            if (onFrameComplete) onFrameComplete();
        };
    }

    // Callback renderowania – wolany po kazdej klatce PPU.
    // Sygnatura: (const uint32_t* fb)
    std::function<void(const uint32_t*)> onRenderFrame;

    // --- NESCoreBase API --------------------------------------------------
    bool loadFile(const std::string& path) override {
        cart = std::make_unique<Cartridge>(path);
        if (!cart->isImageValid()) {
            std::cerr << "[NES] Nie udalo sie zaladowac pliku .nes\n";
            cart.reset();
            return false;
        }
        ppuBus.cart = cart.get();
        pal = false;
        apu.setPAL(pal);
        reset();
        return true;
    }

    std::string windowTitle(const std::string& filename) const override {
        return "NES Emulator - " + filename;
    }

    void shutdown() override {
        controller.closeAndDetachGamepad();
        controller2.closeAndDetachGamepad();
    }

    void onGamepadAdded(uint32_t joystickId) override {
        if (!controller.gamepad()) {
            if (SDL_Gamepad* gp = SDL_OpenGamepad(joystickId)) controller.attachGamepad(gp);
        } else if (!controller2.gamepad()) {
            if (SDL_Gamepad* gp = SDL_OpenGamepad(joystickId)) controller2.attachGamepad(gp);
        }
    }

    void onGamepadRemoved(uint32_t joystickId) override {
        if (controller.gamepadID() == joystickId)  { controller.closeAndDetachGamepad();  return; }
        if (controller2.gamepadID() == joystickId) { controller2.closeAndDetachGamepad(); return; }
    }

    void clockOneCycle(float& outAudioSample) override {
        ppu.clock();
        ppu.clock();
        if (cart) cart->clock();
        apu.clock();

        cpu.tick();
        ppu.clock();

        bool nmiLow = ppu.nmiLineLow();
        bool irqLvl = apu.irqAsserted() || (cart && cart->irqState());
        cpu.setNMILine(nmiLow);
        cpu.setIRQ(irqLvl);

        if (apu.dmcNeedsSample()) {
            uint8_t s = cpuRead(apu.dmcSampleAddress());
            apu.loadDMCSample(s);
            cpu.addStall(4);
        }

        outAudioSample = apu.getOutputSample() + (cart ? cart->audioOutput() : 0.0f);
    }

    void renderFrame() override {
        if (onRenderFrame) onRenderFrame(ppu.getFramebuffer());
    }

    bool hasPPU() const override {
        return true;
    }

    int getCurrentScanline() const override {
        return ppu.getScanline();
    }

private:
    void reset() {
        cpuRam.fill(0x00);
        if (cart) cart->reset();
        ppu.reset();
        cpu.reset();
    }

protected:
    uint8_t cpuRead(uint16_t addr) override {
        uint8_t data;
        if (addr < 0x2000) {
            data = cpuRam[addr & 0x07FF];
        }
        else if (addr < 0x4000) {
            data = ppu.cpuRead(addr);
        }
        else if (addr < 0x4020) {
            if (addr == 0x4015) {
                data = apu.cpuRead(addr);
            }
            else if (addr == 0x4016) {
                uint8_t bit = (controllerShift & 0x80) ? 1 : 0;
                if (!controllerStrobe) controllerShift <<= 1;
                data = (openBus & 0xE0) | bit;
            }
            else if (addr == 0x4017) {
                uint8_t bit = (controllerShift2 & 0x80) ? 1 : 0;
                if (!controllerStrobe) controllerShift2 <<= 1;
                data = (openBus & 0xE0) | bit;
            }
            else {
                data = openBus;
            }
        }
        else {
            data = cart ? cart->cpuRead(addr, openBus) : openBus;
        }
        openBus = data;
        return data;
    }

    void cpuWrite(uint16_t addr, uint8_t data) override {
        openBus = data;
        if (addr < 0x2000) { cpuRam[addr & 0x07FF] = data; return; }
        if (addr < 0x4000) { ppu.cpuWrite(addr, data); return; }
        if (addr < 0x4020) {
            if (addr == 0x4014) {
                uint16_t base = (uint16_t)data << 8;
                for (int b = 0; b < 256; b++)
                    ppu.oamDMAWrite(cpuRead(base + b));
                uint16_t stall = 513 + (~cpu.totalCycles & 1);
                cpu.addStall(stall);
                return;
            }
            if (addr == 0x4016) {
                controllerStrobe = (data & 0x01) != 0;
                if (controllerStrobe) {
                    controllerShift = controller.readState();
                    controllerShift2 = controller2.readState();
                }
                return;
            }
            apu.cpuWrite(addr, data);
            return;
        }
        if (cart) cart->cpuWrite(addr, data);
    }

    PPU2C02 ppu;
    PPUBus  ppuBus;
    std::unique_ptr<Cartridge> cart;

    uint8_t openBus = 0x00;

    NESController controller;
    NESController controller2;
    uint8_t controllerShift = 0x00;
    uint8_t controllerShift2 = 0x00;
    bool    controllerStrobe = false;
};