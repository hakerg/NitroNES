#pragma once
#include <array>
#include <cstdio>
#include <cstring>
#include "Cartridge.h"
#include "DelayedPin.h"
#include "NESConst.h"
#include "Tracer.h"

class PPU2C02 {
public:
    explicit PPU2C02(Cartridge& cart) : cart(cart) {
        buf.fill(0xFF000000);
        std::memset(OAM, 0, sizeof(OAM));
        std::memset(spriteScanline, 0xFF, sizeof(spriteScanline));
        nameTable[0].fill(0x00);
        nameTable[1].fill(0x00);
    }

    Cartridge& cart;

    bool nmiLineLow() const { return ctrl.enableNmi && nmiVbl; }

    int getCompletedFramesCount() { return completedFramesCount; }

    void cpuWrite(uint16_t addr, uint8_t data) {
        refreshOpenBus(0xFF, data);
        switch (addr & 0x0007) {
        case 0:
            ctrl.reg = data;
            tramAddr.nametableX = ctrl.nametableX;
            tramAddr.nametableY = ctrl.nametableY;
            if (!ctrl.enableNmi && scanline == 241
                && (cycle == 1 || cycle == 2 || cycle == 3))
                nmiCycleLatch = false;
            break;
        case 1: {
            const bool disableRendering = renderingEnabled() && !(data & 0x18);
            if (disableRendering && (scanline == 261 || scanline <= 239)) {
                oamCorruptionRow = ((cycle + 1) / 2) & 0x1F;
                oamCorruptionPending = true;
            }
            mask.reg = data;
            break;
        }
        case 2: break;
        case 3: oamAddr = data; break;
        case 4:
            if (renderingEnabled() && (scanline <= 239 || scanline == 261))
                oamAddr = (oamAddr + 4) & 0xFC;
            else
                OAM[oamAddr++] = data;
            break;
        case 5:
            if (!addressLatch) {
                fineX = data & 0x07;
                tramAddr.coarseX = data >> 3;
                addressLatch = 1;
            } else {
                tramAddr.fineY   = data & 0x07;
                tramAddr.coarseY = data >> 3;
                addressLatch = 0;
            }
            break;
        case 6:
            if (!addressLatch) {
                tramAddr.reg = (uint16_t)(((data & 0x3F) << 8) | (tramAddr.reg & 0x00FF));
                addressLatch = 1;
            } else {
                tramAddr.reg = (tramAddr.reg & 0xFF00) | data;
                vramAddr.reg = tramAddr.reg;
                addressLatch = 0;
                drivePpuAddress();
            }
            break;
        case 7:
            ppuWrite(vramAddr.reg, data);
            drivePpuAddress();
            incrementVramAddr();
            drivePpuAddress();
            break;
        default:
            break;
        }
    }

    uint8_t cpuRead(uint16_t addr, bool readOnly = false) {
        switch (addr & 0x0007) {
        case 0: case 1: case 3: case 5: case 6:
            return ppuOpenBus;
        case 2: return readStatus(readOnly);
        case 4: return readOAMData(readOnly);
        case 7: return readPPUData(readOnly);
        default: break;
        }
        return ppuOpenBus;
    }

    void    oamDMAWrite(uint8_t data) { OAM[oamAddr++] = data; }
    uint8_t getOamAddr() const        { return oamAddr; }
    const uint8_t* oamData() const    { return OAM; }

    uint8_t ppuRead(uint16_t addr) {
        addr &= 0x3FFF;
        if (addr >= 0x3F00) {
            uint8_t v = palScreen[paletteIndex(addr)];
            return mask.greyscale ? (v & 0x30) : v;
        }
        if (uint8_t ppuReadBuf = 0; cart.ppuRead(addr, ppuReadBuf)) return ppuReadBuf;
        if (addr >= 0x2000 && addr <= 0x3EFF) {
            addr &= 0x0FFF;
            Mirroring m = cart.getMirroring();
            if (m == Mirroring::VERTICAL)     return nameTable[(addr & 0x0400) >> 10][addr & 0x03FF];
            if (m == Mirroring::HORIZONTAL)   return nameTable[(addr & 0x0800) >> 11][addr & 0x03FF];
            if (m == Mirroring::ONESCREEN_LO) return nameTable[0][addr & 0x03FF];
            if (m == Mirroring::ONESCREEN_HI) return nameTable[1][addr & 0x03FF];
        }
        return 0x00;
    }

    void ppuWrite(uint16_t addr, uint8_t data) {
        addr &= 0x3FFF;
        if (addr >= 0x3F00) { palScreen[paletteIndex(addr)] = data; return; }
        if (cart.ppuWrite(addr, data)) return;
        if (addr >= 0x2000 && addr <= 0x3EFF) {
            addr &= 0x0FFF;
            Mirroring m = cart.getMirroring();
            if (m == Mirroring::VERTICAL)     { nameTable[(addr & 0x0400) >> 10][addr & 0x03FF] = data; return; }
            if (m == Mirroring::HORIZONTAL)   { nameTable[(addr & 0x0800) >> 11][addr & 0x03FF] = data; return; }
            if (m == Mirroring::ONESCREEN_LO) { nameTable[0][addr & 0x03FF] = data; return; }
            if (m == Mirroring::ONESCREEN_HI) { nameTable[1][addr & 0x03FF] = data; return; }
        }
    }

    void reset() {
        fineX = addressLatch = ppuDataBuffer = 0;
        scanline = 261;
        cycle = 0;
        bgNextTileId = bgNextTileAttrib = 0;
        bgNextTilePattern = {};
        bgPattern = {};
        bgAttrib  = {};
        status.reg = mask.reg = ctrl.reg = 0;
        vramAddr.reg = tramAddr.reg = 0;
        oamAddr = 0;
        oamDataBuffer = oamEvalAddr = oamSecondaryIndex = 0;
        oamEvalCopying = false;
        oamCorruptionPending = false;
        oamCorruptionRow = 0;
        frameOdd = false;
        oddFrameSkip.force(false);
        spriteCount = 0;
        spriteZeroHitPossible = false;
        spriteOverflowCycle = -1;
        spriteZeroHitDelay.force(false);
        suppressVblThisFrame = false;
        nmiCycleLatch = false;
        nmiVbl = false;
        ppuOpenBus = 0x00;
        std::memset(spriteScanline, 0xFF, sizeof(spriteScanline));
        for (auto& p : spritePattern) p = {};
    }

    uint32_t* getFramebuffer() { return buf.data(); }

    void clock() {
        cart.clockPpu();
        const bool visible = (scanline >= 0 && scanline <= 239);

        if (oamCorruptionPending && renderingEnabled() && (visible || scanline == 261)) {
            std::memcpy(OAM + oamCorruptionRow * 8, OAM, 8);
            oamCorruptionPending = false;
        }

        if (bool prerender = (scanline == 261); visible || prerender) {
            if (prerender && cycle == 1) {
                status.verticalBlank  = 0;
                status.spriteZeroHit  = 0;
                status.spriteOverflow = 0;
                spriteOverflowCycle   = -1;
                for (auto& p : spritePattern) p = {};
            }

            backgroundFetchPhase();

            if (cycle == 256) incrementScrollY();
            if (cycle == 257) {
                if (renderingEnabled()) loadBackgroundShifters();
                transferAddressX();
            }
            if (cycle == 338 || cycle == 340) {
                if (renderingEnabled())
                    bgNextTileId = busRead(0x2000 | (vramAddr.reg & 0x0FFF));
            }
            if (prerender && cycle >= 280 && cycle < 305) transferAddressY();

            if (visible && cycle == 64)
                spriteOverflowCycle = renderingEnabled() ? computeSpriteOverflowCycle() : -1;
            if (visible && renderingEnabled()) clockOAMEvaluation();
            if (visible && spriteOverflowCycle >= 65 && cycle == spriteOverflowCycle)
                status.spriteOverflow = 1;

            if (visible && cycle == 257 && renderingEnabled()) evaluateSprites();
            spriteFetchPhase();

            if (renderingEnabled() && cycle >= 257 && cycle <= 320) oamAddr = 0;
        }

        if (scanline == 241 && cycle == 1) {
            if (!suppressVblThisFrame) status.verticalBlank = 1;
            suppressVblThisFrame = false;
            ppuOpenBus = 0x00;
        }

        if (scanline == 241 && cycle == 0) nmiVbl = true;
        if (scanline == 261 && cycle == 0) nmiVbl = false;

        renderPixel();

        if (nmiLineLow()) nmiCycleLatch = true;

        advanceCycle();
        emitTrace();
    }

    bool pollNmiLow() {
        bool low = nmiCycleLatch || nmiLineLow();
        nmiCycleLatch = false;
        return low;
    }

    int16_t getScanline() const { return scanline; }
    int16_t getCycle()    const { return cycle; }

    void setTracer(Tracer* t) { tracer = t; }

private:
    Tracer* tracer = nullptr;

    std::array<std::array<uint8_t, 1024>, 2> nameTable{};

    union PPUCTRL {
        struct {
            uint8_t nametableX        : 1;
            uint8_t nametableY        : 1;
            uint8_t incrementMode     : 1;
            uint8_t patternSprite     : 1;
            uint8_t patternBackground : 1;
            uint8_t spriteSize        : 1;
            uint8_t slaveMode         : 1;
            uint8_t enableNmi         : 1;
        };
        uint8_t reg;
    } ctrl{};

    union PPUMASK {
        struct {
            uint8_t greyscale            : 1;
            uint8_t renderBackgroundLeft : 1;
            uint8_t renderSpritesLeft    : 1;
            uint8_t renderBackground     : 1;
            uint8_t renderSprites        : 1;
            uint8_t enhanceRed           : 1;
            uint8_t enhanceGreen         : 1;
            uint8_t enhanceBlue          : 1;
        };
        uint8_t reg;
    } mask{};

    union PPUSTATUS {
        struct {
            uint8_t unused         : 5;
            uint8_t spriteOverflow : 1;
            uint8_t spriteZeroHit  : 1;
            uint8_t verticalBlank  : 1;
        };
        uint8_t reg;
    } status{};

    union loopy {
        struct {
            uint16_t coarseX    : 5;
            uint16_t coarseY    : 5;
            uint16_t nametableX : 1;
            uint16_t nametableY : 1;
            uint16_t fineY      : 3;
            uint16_t unused     : 1;
        };
        uint16_t reg = 0;
    };
    loopy vramAddr;
    loopy tramAddr;

    uint8_t  fineX         = 0;
    uint8_t  addressLatch  = 0;
    uint8_t  ppuDataBuffer = 0;

    int16_t  scanline             = 0;
    int16_t  cycle                = 0;
    bool     frameOdd             = false;
    bool     suppressVblThisFrame = false;
    DelayedPin<bool> oddFrameSkip{false};
    bool     nmiCycleLatch        = false;
    bool     nmiVbl               = false;
    int      completedFramesCount = 0;

    uint8_t  ppuOpenBus = 0x00;

    template <typename T>
    struct ShiftPair { T lo = 0, hi = 0; };

    uint8_t  bgNextTileId     = 0;
    uint8_t  bgNextTileAttrib = 0;
    ShiftPair<uint8_t> bgNextTilePattern{};

    ShiftPair<uint16_t> bgPattern{};
    ShiftPair<uint16_t> bgAttrib{};

    uint8_t  OAM[256]{};
    uint8_t  oamAddr = 0;
    uint8_t  oamDataBuffer = 0;
    uint8_t  oamEvalAddr = 0;
    uint8_t  oamSecondaryIndex = 0;
    bool     oamEvalCopying = false;
    bool     oamCorruptionPending = false;
    uint8_t  oamCorruptionRow = 0;
    uint8_t  spriteScanline[8 * 4]{};
    uint8_t  spriteCount = 0;
    ShiftPair<uint8_t> spritePattern[8]{};
    bool     spriteZeroHitPossible = false;
    int16_t  spriteOverflowCycle   = -1;

    ShiftDelay<bool, 2> spriteZeroHitDelay{false};

    std::array<uint8_t, 32>          palScreen;
    std::array<uint32_t, 256 * 240>  buf;

    static uint16_t paletteIndex(uint16_t addr) {
        addr &= 0x001F;
        if (addr == 0x10 || addr == 0x14 || addr == 0x18 || addr == 0x1C) addr &= 0x000F;
        return addr;
    }

    bool renderingEnabled() const { return mask.renderBackground || mask.renderSprites; }

    void refreshOpenBus(uint8_t maskBits, uint8_t value) {
        ppuOpenBus = (ppuOpenBus & ~maskBits) | (value & maskBits);
    }

    uint8_t readStatus(bool readOnly) {
        uint8_t ppuBits = status.reg & 0xE0;
        uint8_t data = ppuBits | (ppuOpenBus & 0x1F);
        if (!readOnly) {
            if (scanline == 241) {
                if (cycle == 1) {
                    suppressVblThisFrame = true;
                }
                if (cycle == 1 || cycle == 2 || cycle == 3) {
                    nmiCycleLatch = false;
                }
            }
            status.verticalBlank = 0;
            nmiVbl = false;
            addressLatch = 0;
            refreshOpenBus(0xE0, ppuBits);
        }
        return data;
    }

    uint8_t readOAMData(bool readOnly) {
        if (renderingEnabled() && scanline <= 239) {
            if ((cycle >= 1 && cycle <= 64) || (cycle >= 257 && cycle <= 320)) {
                if (!readOnly) refreshOpenBus(0xFF, 0xFF);
                return 0xFF;
            }
            if (cycle >= 65 && cycle <= 256) {
                if (!readOnly) refreshOpenBus(0xFF, oamDataBuffer);
                return oamDataBuffer;
            }
        }
        uint8_t data = OAM[oamAddr];
        if ((oamAddr & 0x03) == 0x02) data &= 0xE3;
        if (!readOnly) refreshOpenBus(0xFF, data);
        return data;
    }

    void clockOAMEvaluation() {
        if (cycle <= 64) {
            oamDataBuffer = 0xFF;
            return;
        }
        if (cycle > 256) return;
        if (cycle == 65) {
            oamEvalAddr = oamAddr;
            oamSecondaryIndex = 0;
            oamEvalCopying = false;
        }
        if (cycle & 1) {
            oamDataBuffer = OAM[oamEvalAddr];
            if ((oamEvalAddr & 3) == 2) oamDataBuffer &= 0xE3;
            return;
        }
        if (oamSecondaryIndex < 32) {
            if (!oamEvalCopying) {
                const int row = scanline - oamDataBuffer;
                if (row < 0 || row >= (ctrl.spriteSize ? 16 : 8)) {
                    oamEvalAddr = (oamEvalAddr + 4) & 0xFC;
                    return;
                }
                oamEvalCopying = true;
            }
            oamSecondaryIndex++;
            oamEvalAddr++;
            if ((oamEvalAddr & 3) == 0) oamEvalCopying = false;
            return;
        }
        const int row = scanline - oamDataBuffer;
        oamEvalAddr += row >= 0 && row < (ctrl.spriteSize ? 16 : 8) ? 4 : 5;
    }

    uint8_t readPPUData(bool readOnly) {
        uint8_t data;
        if (uint16_t busAddr = vramAddr.reg & 0x3FFF; busAddr >= 0x3F00) {
            uint8_t pal = ppuRead(busAddr) & 0x3F;
            data = (ppuOpenBus & 0xC0) | pal;
            ppuDataBuffer = ppuRead(busAddr & 0x2FFF);
            if (!readOnly) refreshOpenBus(0x3F, pal);
        } else {
            data = ppuDataBuffer;
            ppuDataBuffer = ppuRead(busAddr);
            if (!readOnly) refreshOpenBus(0xFF, data);
        }
        if (!readOnly) {
            drivePpuAddress();
            incrementVramAddr();
            drivePpuAddress();
        }
        return data;
    }

    void backgroundFetchPhase() {
        if (!renderingEnabled()) return;
        if (!((cycle >= 2 && cycle < 258) || (cycle >= 321 && cycle < 338))) return;

        updateShifters();

        switch ((cycle - 1) & 7) {
        case 0:
            loadBackgroundShifters();
            bgNextTileId = busRead(0x2000 | (vramAddr.reg & 0x0FFF));
            break;
        case 2: {
            uint16_t at = 0x23C0
                | (vramAddr.nametableY << 11)
                | (vramAddr.nametableX << 10)
                | ((vramAddr.coarseY >> 2) << 3)
                | (vramAddr.coarseX >> 2);
            bgNextTileAttrib = busRead(at);
            if (vramAddr.coarseY & 0x02) bgNextTileAttrib >>= 4;
            if (vramAddr.coarseX & 0x02) bgNextTileAttrib >>= 2;
            bgNextTileAttrib &= 0x03;
            break;
        }
        case 4: {
            const uint16_t base = ((uint16_t)ctrl.patternBackground << 12)
                + ((uint16_t)bgNextTileId << 4) + vramAddr.fineY;
            bgNextTilePattern.lo = busRead(base);
            break;
        }
        case 6: {
            const uint16_t base = ((uint16_t)ctrl.patternBackground << 12)
                + ((uint16_t)bgNextTileId << 4) + vramAddr.fineY;
            bgNextTilePattern.hi = busRead(base + 8);
            break;
        }
        case 7:
            incrementScrollX();
            break;
        default:
            break;
        }
    }

    void loadBackgroundShifters() {
        bgPattern.lo = (bgPattern.lo & 0xFF00) | bgNextTilePattern.lo;
        bgPattern.hi = (bgPattern.hi & 0xFF00) | bgNextTilePattern.hi;
        bgAttrib.lo  = (bgAttrib.lo  & 0xFF00) | ((bgNextTileAttrib & 0x1) ? 0xFF : 0x00);
        bgAttrib.hi  = (bgAttrib.hi  & 0xFF00) | ((bgNextTileAttrib & 0x2) ? 0xFF : 0x00);
    }

    void updateShifters() {
        if (renderingEnabled()) {
            bgPattern.lo <<= 1;
            bgPattern.hi = (bgPattern.hi << 1) | 0x0001;
            bgAttrib.lo  <<= 1;
            bgAttrib.hi  <<= 1;
        }
        if (renderingEnabled() && cycle >= 1 && cycle < 258) {
            const bool visibleScanline = (scanline >= 0 && scanline <= 239);
            for (int i = 0; i < spriteCount; i++) {
                if (spriteScanline[i * 4 + 3] > 0)
                    spriteScanline[i * 4 + 3]--;
                else if (visibleScanline) {
                    spritePattern[i].lo <<= 1;
                    spritePattern[i].hi <<= 1;
                }
            }
        }
    }

    void incrementScrollX() {
        if (!renderingEnabled()) return;
        if (vramAddr.coarseX == 31) {
            vramAddr.coarseX = 0;
            vramAddr.nametableX = ~vramAddr.nametableX;
        } else {
            vramAddr.coarseX++;
        }
    }

    void incrementScrollY() {
        if (!renderingEnabled()) return;
        if (vramAddr.fineY < 7) {
            vramAddr.fineY++;
        } else {
            vramAddr.fineY = 0;
            if (vramAddr.coarseY == 29) {
                vramAddr.coarseY = 0;
                vramAddr.nametableY = ~vramAddr.nametableY;
            } else if (vramAddr.coarseY == 31) {
                vramAddr.coarseY = 0;
            } else {
                vramAddr.coarseY++;
            }
        }
    }

    void transferAddressX() {
        if (!renderingEnabled()) return;
        vramAddr.nametableX = tramAddr.nametableX;
        vramAddr.coarseX    = tramAddr.coarseX;
    }

    void transferAddressY() {
        if (!renderingEnabled()) return;
        vramAddr.fineY      = tramAddr.fineY;
        vramAddr.nametableY = tramAddr.nametableY;
        vramAddr.coarseY    = tramAddr.coarseY;
    }

    void incrementVramAddr() {
        if (renderingEnabled() && (scanline == 261 || scanline <= 239)) {
            incrementScrollY();
            incrementScrollX();
        } else {
            vramAddr.reg += (ctrl.incrementMode ? 32 : 1);
        }
    }

    int16_t computeSpriteOverflowCycle() const {
        const int spriteH = ctrl.spriteSize ? 16 : 8;
        int dot = 65, count = 0, m = 0;
        for (int n = 0; n < 64; n++) {
            if (dot > 256) break;
            if (count < 8) {
                if (int16_t diff = (int16_t)scanline - (int16_t)OAM[n * 4];
                    diff >= 0 && diff < spriteH) { count++; dot += 8; }
                else dot += 2;
            } else {
                if (int16_t diff = (int16_t)scanline - (int16_t)OAM[n * 4 + m];
                    diff >= 0 && diff < spriteH) return (int16_t)dot;
                m = (m + 1) & 3;
                dot += 2;
            }
        }
        return -1;
    }

    void evaluateSprites() {
        std::memset(spriteScanline, 0xFF, sizeof(spriteScanline));
        spriteCount = 0;
        for (auto& p : spritePattern) p = {};
        spriteZeroHitPossible = false;

        const int spriteH = ctrl.spriteSize ? 16 : 8;
        uint8_t addr = oamAddr;
        for (int n = 0; n < 64; ++n, addr += 4) {
            if (int16_t diff = scanline - (int16_t)OAM[addr];
                diff < 0 || diff >= spriteH) continue;
            if (spriteCount < 8) {
                if (n == 0) spriteZeroHitPossible = true;
                for (int k = 0; k < 4; k++) {
                    uint8_t b = OAM[(uint8_t)(addr + k)];
                    if (((addr + k) & 0x03) == 0x02) b &= 0xE3;
                    spriteScanline[spriteCount * 4 + k] = b;
                }
                spriteCount++;
            }
        }
    }

    uint16_t spritePatternAddress(uint8_t tile, uint8_t attr, int row) const {
        const bool flipV = (attr & 0x80) != 0;
        if (!ctrl.spriteSize) {
            int r = flipV ? (7 - row) : row;
            return ((uint16_t)ctrl.patternSprite << 12) | ((uint16_t)tile << 4) | (uint16_t)(r & 0x07);
        }
        const uint16_t patTable = ((uint16_t)tile & 0x01) << 12;
        const uint16_t topTile  = (uint16_t)tile & 0xFE;
        int r = flipV ? (15 - row) : row;
        return patTable | (((r < 8) ? topTile : (topTile + 1)) << 4) | (uint16_t)(r & 0x07);
    }

    void loadSpritePatterns() {
        for (uint8_t i = 0; i < 8; i++) {
            uint8_t y    = (i < spriteCount) ? spriteScanline[i * 4 + 0] : 0xFF;
            uint8_t tile = (i < spriteCount) ? spriteScanline[i * 4 + 1] : 0xFF;
            uint8_t attr = (i < spriteCount) ? spriteScanline[i * 4 + 2] : 0xFF;
            uint16_t addrLo = spritePatternAddress(tile, attr, scanline - y);
            uint8_t lo = busRead(addrLo);
            uint8_t hi = busRead(addrLo + 8);
            if (i >= spriteCount) continue;
            if (attr & 0x40) { lo = flipByte(lo); hi = flipByte(hi); }
            spritePattern[i].lo = lo;
            spritePattern[i].hi = hi;
        }
    }

    void spriteFetchPhase() {
        if (!renderingEnabled()) return;
        if (cycle < 257 || cycle > 320) return;

        const bool visible = (scanline >= 0 && scanline <= 239);
        const int spriteIdx = (cycle - 257) / 8;
        const int phase     = (cycle - 257) & 7;

        if (phase == 0 || phase == 2) {
            busRead(0x2000 | (vramAddr.reg & 0x0FFF));
            return;
        }
        if (phase != 4 && phase != 6) return;

        const bool active = visible && spriteIdx < spriteCount;
        const uint8_t y    = active ? spriteScanline[spriteIdx * 4 + 0] : 0xFF;
        const uint8_t tile = active ? spriteScanline[spriteIdx * 4 + 1] : 0xFF;
        const uint8_t attr = active ? spriteScanline[spriteIdx * 4 + 2] : 0xFF;
        const uint16_t base = spritePatternAddress(tile, attr, scanline - y);
        const uint8_t data = busRead(base + (phase == 6 ? 8 : 0));
        if (!active) return;
        const uint8_t v = (attr & 0x40) ? flipByte(data) : data;
        if (phase == 4) spritePattern[spriteIdx].lo = v;
        else            spritePattern[spriteIdx].hi = v;
    }

    struct Pixel { uint8_t color; uint8_t palette; bool nonzero; };

    Pixel sampleBackground() const {
        if (!mask.renderBackground) return { 0, 0, false };
        if (!mask.renderBackgroundLeft && cycle < 9) return { 0, 0, false };
        const uint16_t mux = 0x8000u >> fineX;
        const uint8_t p0  = (bgPattern.lo & mux) ? 1 : 0;
        const uint8_t p1  = (bgPattern.hi & mux) ? 1 : 0;
        const uint8_t a0  = (bgAttrib.lo  & mux) ? 1 : 0;
        const uint8_t a1  = (bgAttrib.hi  & mux) ? 1 : 0;
        const auto col = (uint8_t)((p1 << 1) | p0);
        return { col, (uint8_t)((a1 << 1) | a0), col != 0 };
    }

    Pixel sampleSprite(bool& spriteZeroOut, bool& fgPriorityOut) {
        spriteZeroOut = false;
        fgPriorityOut = false;
        if (!mask.renderSprites) return { 0, 0, false };
        if (!mask.renderSpritesLeft && cycle < 9) return { 0, 0, false };
        for (uint8_t i = 0; i < spriteCount; i++) {
            if (spriteScanline[i * 4 + 3] != 0) continue;
            const uint8_t lo  = (spritePattern[i].lo & 0x80) ? 1 : 0;
            const uint8_t hi  = (spritePattern[i].hi & 0x80) ? 1 : 0;
            const auto col = (uint8_t)((hi << 1) | lo);
            if (col == 0) continue;
            const uint8_t attr = spriteScanline[i * 4 + 2];
            fgPriorityOut = (attr & 0x20) == 0;
            spriteZeroOut = (i == 0);
            return { col, (uint8_t)((attr & 0x03) + 0x04), true };
        }
        return { 0, 0, false };
    }

    void renderPixel() {
        const int x = cycle - 1;
        const int y = scanline;
        if ((unsigned)x >= (unsigned)NES::SCREEN_WIDTH || (unsigned)y >= (unsigned)NES::SCREEN_HEIGHT) {
            status.spriteZeroHit |= spriteZeroHitDelay.tick(false);
            return;
        }

        Pixel bg = sampleBackground();
        bool spriteZero = false, fgPriority = false;
        Pixel sp = sampleSprite(spriteZero, fgPriority);
        const bool isSpriteZeroPixel = spriteZero && sp.nonzero;

        bool hitNow = false;
        uint8_t pixel = 0, paletteIdx = 0;
        if (!bg.nonzero && sp.nonzero) {
            pixel = sp.color; paletteIdx = sp.palette;
        } else if (bg.nonzero && !sp.nonzero) {
            pixel = bg.color; paletteIdx = bg.palette;
        } else if (bg.nonzero && sp.nonzero) {
            if (fgPriority) { pixel = sp.color; paletteIdx = sp.palette; }
            else            { pixel = bg.color; paletteIdx = bg.palette; }
            if (spriteZeroHitPossible && isSpriteZeroPixel
                && mask.renderBackground && mask.renderSprites) {
                const int xLo = (mask.renderBackgroundLeft && mask.renderSpritesLeft) ? 2 : 9;
                if (cycle >= xLo && cycle < 256) hitNow = true;
            }
        }
        status.spriteZeroHit |= spriteZeroHitDelay.tick(hitNow);

        uint8_t idx;
        if (!renderingEnabled() && (vramAddr.reg & 0x3F00) == 0x3F00)
            idx = palScreen[paletteIndex(vramAddr.reg)];
        else
            idx = palScreen[paletteIndex(0x3F00u | ((uint16_t)paletteIdx << 2) | pixel)];
        if (mask.greyscale) idx &= 0x30;
        auto emph = (uint8_t)((mask.enhanceBlue << 2) | (mask.enhanceGreen << 1) | mask.enhanceRed);
        buf[y * NES::SCREEN_WIDTH + x] = emphasisLUT()[emph][idx & 0x3F];
    }

    void advanceCycle() {
        if (scanline == 261 && cycle == 338)
            oddFrameSkip.set(frameOdd && renderingEnabled(), 2);
        oddFrameSkip.tick();

        cycle++;

        if (scanline == 261 && cycle == 340 && oddFrameSkip.get()) {
            cycle    = 0;
            scanline = 0;
            oddFrameSkip.force(false);
            frameOdd = !frameOdd;
        } else if (cycle >= 341) {
            cycle = 0;
            if (scanline == 239) completedFramesCount++;
            scanline++;
            if (scanline >= 262) {
                scanline = 0;
                frameOdd = !frameOdd;
            }
        }
    }

    static const std::array<uint32_t, 64>& palette() {
        static constexpr std::array lut = {
            0xFF6A6D6A, 0xFF001380, 0xFF1E008A, 0xFF39007A,
            0xFF550056, 0xFF5A0018, 0xFF4F1000, 0xFF3D1C00,
            0xFF253200, 0xFF003D00, 0xFF004000, 0xFF003924,
            0xFF002E55, 0xFF000000, 0xFF000000, 0xFF000000,
            0xFFB9BCB9, 0xFF1850C7, 0xFF4B30E3, 0xFF7322D6,
            0xFF951FA9, 0xFF9D285C, 0xFF983700, 0xFF7F4C00,
            0xFF5E6400, 0xFF227700, 0xFF027E02, 0xFF007645,
            0xFF006E8A, 0xFF000000, 0xFF000000, 0xFF000000,
            0xFFFFFFFF, 0xFF68A6FF, 0xFF8C9CFF, 0xFFB586FF,
            0xFFD975FD, 0xFFE377B9, 0xFFE58D68, 0xFFD49D29,
            0xFFB3AF0C, 0xFF7BC211, 0xFF55CA47, 0xFF46CB81,
            0xFF47C1C5, 0xFF4A4D4A, 0xFF000000, 0xFF000000,
            0xFFFFFFFF, 0xFFCCEAFF, 0xFFDDDEFF, 0xFFECDAFF,
            0xFFF8D7FE, 0xFFFCD6F5, 0xFFFDDBCF, 0xFFF9E7B5,
            0xFFF1F0AA, 0xFFDAFAA9, 0xFFC9FFBC, 0xFFC3FBD7,
            0xFFC4F6F6, 0xFFBEC1BE, 0xFF000000, 0xFF000000,
        };
        return lut;
    }

    static const std::array<std::array<uint32_t, 64>, 8>& emphasisLUT() {
        static const auto lut = []() {
            std::array<std::array<uint32_t, 64>, 8> t{};
            const auto& pal = palette();
            for (int emph = 0; emph < 8; ++emph) {
                const bool r = (emph & 1) != 0;
                const bool g = (emph & 2) != 0;
                const bool b = (emph & 4) != 0;
                for (int i = 0; i < 64; ++i) {
                    uint32_t argb = pal[i];
                    if (r || g || b) {
                        uint32_t cr = (argb >> 16) & 0xFF;
                        uint32_t cg = (argb >>  8) & 0xFF;
                        uint32_t cb =  argb         & 0xFF;
                        if (!r) cr = (cr * 3) >> 2;
                        if (!g) cg = (cg * 3) >> 2;
                        if (!b) cb = (cb * 3) >> 2;
                        argb = (argb & 0xFF000000u) | (cr << 16) | (cg << 8) | cb;
                    }
                    t[emph][i] = argb;
                }
            }
            return t;
        }();
        return lut;
    }

    static uint8_t flipByte(uint8_t b) {
        b = (uint8_t)(((b & 0xF0) >> 4) | ((b & 0x0F) << 4));
        b = (uint8_t)(((b & 0xCC) >> 2) | ((b & 0x33) << 2));
        b = (uint8_t)(((b & 0xAA) >> 1) | ((b & 0x55) << 1));
        return b;
    }

    uint8_t busRead(uint16_t addr) {
        addr &= 0x3FFF;
        cart.ppuAddress(addr);
        return ppuRead(addr);
    }

    void drivePpuAddress() {
        cart.ppuAddress(vramAddr.reg & 0x3FFF);
    }

    const char* phaseName() const {
        if (scanline == 261)              return "PRE";
        if (scanline >= 0 && scanline <= 239) {
            if (cycle == 0)               return "IDLE";
            if (cycle <= 256)             return "BG-FETCH";
            if (cycle <= 320)             return "SPR-FETCH";
            if (cycle <= 336)             return "BG-PREFTCH";
            return "NT-DUMMY";
        }
        if (scanline == 240)              return "POST";
        if (scanline >= 241 && scanline <= 260) return "VBLANK";
        return "?";
    }

    void emitTrace() {
        if (!tracer || !tracer->ppu) return;
        char str[256];
        std::snprintf(str, sizeof(str),
            "PPU[SL=%3d,CY=%3d] %-10s V=%04X T=%04X fX=%u W=%u "
            "CTRL=%02X MASK=%02X STAT=%02X OAMA=%02X SPR=%u NMI=%u%s",
            (int)scanline, (int)cycle, phaseName(),
            (unsigned)vramAddr.reg, (unsigned)tramAddr.reg,
            (unsigned)fineX, (unsigned)addressLatch,
            (unsigned)ctrl.reg, (unsigned)mask.reg, (unsigned)status.reg,
            (unsigned)oamAddr, (unsigned)spriteCount,
            nmiLineLow() ? 1u : 0u,
            frameOdd ? " ODD" : "");
        tracer->writePpu(str);
    }
};
