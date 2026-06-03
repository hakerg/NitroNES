#pragma once
#include <cstdint>
#include <array>
#include <atomic>
#include <cstring>
#include <cmath>
#include <memory>
#include <iostream>
#include <SDL3/SDL.h>
#include <windows.h>
#include <GL/glew.h>

#include "NESConst.h"
#include "NESCoreBase.h"
#include "Cartridge.h"
#include "PPUBus.h"
#include "PPU2C02.h"
#include "NESController.h"
#include "MonitorRefreshRateDetector.h"

// Rdzen NES: PPU, kontrolery, beam racer, render tekstury PPU.
class NESSystem : public NESCoreBase {
public:
    static constexpr int FB_W = PPU2C02::SCREEN_WIDTH;
    static constexpr int FB_H = PPU2C02::SCREEN_HEIGHT;

    NESSystem() {
        ppu.ppuBus = &ppuBus;
        cpu.setIrqAckCallback([this]() { if (cart) cart->irqClear(); });
        ppu.onFrameEnd = [this]() {
            controller.tickFrame();
            controller2.tickFrame();
            renderFrame();
        };

        // Gracz 2: strzalki + KP_8/9/0/-, Delete/End.
        controller2.key_A = SDL_SCANCODE_KP_8;
        controller2.key_B = SDL_SCANCODE_KP_9;
        controller2.key_TurboA = SDL_SCANCODE_KP_0;
        controller2.key_TurboB = SDL_SCANCODE_KP_MINUS;
        controller2.key_Select = SDL_SCANCODE_DELETE;
        controller2.key_Start = SDL_SCANCODE_END;
        controller2.key_Up = SDL_SCANCODE_UP;
        controller2.key_Down = SDL_SCANCODE_DOWN;
        controller2.key_Left = SDL_SCANCODE_LEFT;
        controller2.key_Right = SDL_SCANCODE_RIGHT;
    }

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
    void defaultWindowSize(int& w, int& h) const override {
        w = (FB_W * NES::PAR_NUM * 3 + NES::PAR_DEN / 2) / NES::PAR_DEN;
        h = NES::VISIBLE_H * 3;
    }
    bool windowResizable() const override { return true; }

    void initVideo(SDL_Window* window) override {
        this->window = window;
        hwnd = (HWND)SDL_GetPointerProperty(SDL_GetWindowProperties(window),
            SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
        refreshDetector.setWindow(hwnd);

        glewExperimental = GL_TRUE;
        GLenum glewErr = glewInit();
        if (glewErr != GLEW_OK) {
            std::cerr << "[NES] glewInit nieudane: "
                      << (const char*)glewGetErrorString(glewErr) << "\n";
        }

        // Single-buffer GL: tworzymy jedna teksture RGBA do uploadu framebuffera PPU.
        glGenTextures(1, &glTexture);
        glBindTexture(GL_TEXTURE_2D, glTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, FB_W, FB_H, 0,
            GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, nullptr);
    }

    void shutdown() override {
        if (controller.gamepad()) { SDL_CloseGamepad(controller.gamepad());  controller.detachGamepad(); }
        if (controller2.gamepad()) { SDL_CloseGamepad(controller2.gamepad()); controller2.detachGamepad(); }
        if (glTexture) { glDeleteTextures(1, &glTexture); glTexture = 0; }
    }

    bool handleEvent(const SDL_Event& ev, bool& /*paused*/) override {
        switch (ev.type) {
        case SDL_EVENT_GAMEPAD_ADDED: {
            SDL_JoystickID jid = ev.gdevice.which;
            if (!controller.gamepad()) {
                if (SDL_Gamepad* gp = SDL_OpenGamepad(jid)) controller.attachGamepad(gp);
            }
            else if (!controller2.gamepad()) {
                if (SDL_Gamepad* gp = SDL_OpenGamepad(jid)) controller2.attachGamepad(gp);
            }
            return true;
        }
        case SDL_EVENT_GAMEPAD_REMOVED: {
            SDL_JoystickID jid = ev.gdevice.which;
            if (controller.gamepad() && SDL_GetGamepadID(controller.gamepad()) == jid) {
                SDL_CloseGamepad(controller.gamepad()); controller.detachGamepad();
            }
            else if (controller2.gamepad() && SDL_GetGamepadID(controller2.gamepad()) == jid) {
                SDL_CloseGamepad(controller2.gamepad()); controller2.detachGamepad();
            }
            return true;
        }
        default: break;
        }
        return false;
    }

    void clockOneCycle(float& outSample, double& outDt) override {
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

        outSample = apu.getOutputSample() + (cart ? cart->audioOutput() : 0.0f);
        const double hwDt = 1.0 / (pal ? NES::CPU_CLOCK_PAL : NES::CPU_CLOCK_NTSC);
        double speed = getSpeed();
        outDt = hwDt / speed; // TODO: move this logic to NESCoreBase
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

    MonitorRefreshRateDetector refreshDetector;

    HWND             hwnd      = nullptr;
    SDL_Window*      window    = nullptr;
    GLuint           glTexture = 0;

    // Wołane z głównego wątku: uploaduje framebuffer PPU do tekstury GL i prezentuje.
    void renderFrame() {
        if (!window || !glTexture) return;

        int winW = 0, winH = 0;
        SDL_GetWindowSizeInPixels(window, &winW, &winH);
        if (winW <= 0 || winH <= 0) return;

        // Upload tylko widocznego pasa (z pominieciem overscan top/bottom).
        const uint32_t* fb = ppu.getFramebuffer();
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, FB_W);
        glBindTexture(GL_TEXTURE_2D, glTexture);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, FB_W, FB_H,
            GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, fb);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

        // Czarne tlo, viewport na cale okno.
        glViewport(0, 0, winW, winH);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // Wyliczamy docelowy prostokat z zachowaniem PAR i overscan.
        const float targetAspect =
            (float)(FB_W * NES::PAR_NUM) / (float)(NES::VISIBLE_H * NES::PAR_DEN);
        float dstW = (float)winW;
        float dstH = dstW / targetAspect;
        if (dstH > (float)winH) { dstH = (float)winH; dstW = dstH * targetAspect; }
        float dstX = ((float)winW - dstW) * 0.5f;
        float dstY = ((float)winH - dstH) * 0.5f;

        // Konwersja na NDC.
        float x0 =  (dstX            / (float)winW) * 2.0f - 1.0f;
        float x1 = ((dstX + dstW)    / (float)winW) * 2.0f - 1.0f;
        // GL ma Y rosnace w gore - odwracamy tak, by tekstura nie byla "do gory nogami".
        float y1 = 1.0f - ( dstY            / (float)winH) * 2.0f;
        float y0 = 1.0f - ((dstY + dstH)    / (float)winH) * 2.0f;

        // Wspolrzedne tekstury: pomijamy OVERSCAN_TOP/BOTTOM.
        float u0 = 0.0f;
        float u1 = 1.0f;
        float v0 = (float)NES::OVERSCAN_TOP / (float)FB_H;
        float v1 = (float)(NES::OVERSCAN_TOP + NES::VISIBLE_H) / (float)FB_H;

        glMatrixMode(GL_PROJECTION); glLoadIdentity();
        glMatrixMode(GL_MODELVIEW);  glLoadIdentity();

        glEnable(GL_TEXTURE_2D);
        glDisable(GL_BLEND);
        glDisable(GL_DEPTH_TEST);
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

        glBegin(GL_TRIANGLE_STRIP);
            glTexCoord2f(u0, v1); glVertex2f(x0, y0);
            glTexCoord2f(u1, v1); glVertex2f(x1, y0);
            glTexCoord2f(u0, v0); glVertex2f(x0, y1);
            glTexCoord2f(u1, v0); glVertex2f(x1, y1);
        glEnd();

        glDisable(GL_TEXTURE_2D);

        // Single buffer: glFlush wymusza dotarcie polecen do front bufera.
        glFlush();
    }
};