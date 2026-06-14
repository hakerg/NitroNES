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

class NESSystem : public NESCoreBase, public ICPUBus {
public:
    explicit NESSystem(IEmulatorHost& host, INESSystemHost& nesHost, const std::string& path)
        : NESCoreBase(*this, host), ppu(*this), nesHost(nesHost) {
        cart = std::make_unique<Cartridge>(path);
        if (!cart->isImageValid()) {
            cart.reset();
            throw std::runtime_error("[NES] Nie udalo sie zaladowac: " + path);
        }
        ppu.cart = cart.get();
        pal = false;
        apu.setPAL(pal);
        // Power-on: CPU w stanie pre-reset → reset() da S=$FD, P=$34
        cpuRam.fill(0x00);
        cpu.A = 0; cpu.X = 0; cpu.Y = 0;
        cpu.S = 0x00;
        cpu.P = CPU6502::FLAG_U | CPU6502::FLAG_B;
        reset();
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

    void reset() override {
        if (cart) cart->reset();
        apu.reset();
        ppu.reset();
        cpu.reset();
    }

    void renderFrame() override {
        nesHost.renderFrame(ppu.getFramebuffer());
    }

    bool hasPPU() const override {
        return true;
    }

    int getCurrentScanline() const override {
        return ppu.getScanline();
    }

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
                // $4015 nie aktualizuje open bus — zwracamy wynik bez zapisu do openBus
                return apu.cpuRead(addr, openBus);
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
                    controllerShift  = nesHost.readController(0);
                    controllerShift2 = nesHost.readController(1);
                }
                return;
            }
            apu.cpuWrite(addr, data);
            return;
        }
        if (cart) cart->cpuWrite(addr, data);
    }

    void cpuIrqAck() override {
        if (cart) cart->irqClear();
    }

protected:
    PPU2C02 ppu;
    std::unique_ptr<Cartridge> cart;

    uint8_t openBus = 0x00;

    uint8_t controllerShift  = 0x00;
    uint8_t controllerShift2 = 0x00;
    bool    controllerStrobe = false;

private:
    INESSystemHost& nesHost;
};