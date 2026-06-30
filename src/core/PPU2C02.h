#pragma once
#include <array>
#include <cstdio>
#include <cstring>
#include "Cartridge.h"
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

    bool nmiLineLow() const { return ctrl.enable_nmi && nmiVbl; }

    int getCompletedFramesCount() { return completedFramesCount; }

    void cpuWrite(uint16_t addr, uint8_t data) {
        refreshOpenBus(0xFF, data);
        switch (addr & 0x0007) {
        case 0:
            ctrl.reg = data;
            tram_addr.nametable_x = ctrl.nametable_x;
            tram_addr.nametable_y = ctrl.nametable_y;
            if (!ctrl.enable_nmi && scanline == NES::SCANLINE_VBLANK_START
                && (cycle == 1 || cycle == 2 || cycle == 3))
                nmiCycleLatch = false;
            break;
        case 1:
            mask.reg = data;
            break;
        case 2: break;
        case 3: oamAddr = data; break;
        case 4:
            if (renderingEnabled() && (scanline <= NES::SCANLINE_VISIBLE_LAST || scanline == NES::SCANLINE_PRERENDER))
                oamAddr = (oamAddr + 4) & 0xFC;
            else
                OAM[oamAddr++] = data;
            break;
        case 5:
            if (!address_latch) {
                fine_x = data & 0x07;
                tram_addr.coarse_x = data >> 3;
                address_latch = 1;
            } else {
                tram_addr.fine_y   = data & 0x07;
                tram_addr.coarse_y = data >> 3;
                address_latch = 0;
            }
            break;
        case 6:
            if (!address_latch) {
                tram_addr.reg = (uint16_t)(((data & 0x3F) << 8) | (tram_addr.reg & 0x00FF));
                address_latch = 1;
            } else {
                tram_addr.reg = (tram_addr.reg & 0xFF00) | data;
                vram_addr.reg = tram_addr.reg;
                address_latch = 0;
                clockMapperA12();
            }
            break;
        case 7:
            ppuWrite(vram_addr.reg, data);
            clockMapperA12();
            incrementVramAddr();
            clockMapperA12();
            break;
        default:
            break;
        }
    }

    uint8_t cpuRead(uint16_t addr, bool bReadOnly = false) {
        switch (addr & 0x0007) {
        case 0: case 1: case 3: case 5: case 6:
            return ppuOpenBus;
        case 2: return readStatus(bReadOnly);
        case 4: return readOAMData(bReadOnly);
        case 7: return readPPUData(bReadOnly);
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
        uint8_t ppuReadBuf = 0;
        if (cart.ppuRead(addr, ppuReadBuf)) return ppuReadBuf;
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
        fine_x = address_latch = ppu_data_buffer = 0;
        scanline = NES::SCANLINE_PRERENDER;
        cycle = 0;
        bg_next_tile_id = bg_next_tile_attrib = 0;
        bg_next_tile_lsb = bg_next_tile_msb = 0;
        bg_shifter_pattern_lo = bg_shifter_pattern_hi = 0;
        bg_shifter_attrib_lo  = bg_shifter_attrib_hi  = 0;
        status.reg = mask.reg = ctrl.reg = 0;
        vram_addr.reg = tram_addr.reg = 0;
        oamAddr = 0;
        frame_odd = odd_frame_skip = false;
        totalPpuCycle = 0;
        sprite_count = 0;
        bSpriteZeroHitPossible = false;
        sprite_overflow_cycle  = -1;
        suppressVblThisFrame   = false;
        nmiCycleLatch          = false;
        nmiVbl                 = false;
        ppuOpenBus = 0x00;
        std::memset(spriteScanline,           0xFF, sizeof(spriteScanline));
        std::memset(sprite_shifter_pattern_lo, 0,   sizeof(sprite_shifter_pattern_lo));
        std::memset(sprite_shifter_pattern_hi, 0,   sizeof(sprite_shifter_pattern_hi));
    }

    uint32_t* getFramebuffer() { return buf.data(); }

    void clock() {
        const bool visible   = (scanline >= NES::SCANLINE_VISIBLE_FIRST && scanline <= NES::SCANLINE_VISIBLE_LAST);
        const bool prerender = (scanline == NES::SCANLINE_PRERENDER);

        if (visible || prerender) {
            if (prerender && cycle == 1) {
                status.vertical_blank  = 0;
                status.sprite_zero_hit = 0;
                status.sprite_overflow = 0;
                sprite_overflow_cycle  = -1;
                std::memset(sprite_shifter_pattern_lo, 0, sizeof(sprite_shifter_pattern_lo));
                std::memset(sprite_shifter_pattern_hi, 0, sizeof(sprite_shifter_pattern_hi));
            }

            backgroundFetchPhase();

            if (cycle == 256) IncrementScrollY();
            if (cycle == 257) {
                if (renderingEnabled()) LoadBackgroundShifters();
                TransferAddressX();
            }
            if (cycle == 338 || cycle == 340) {
                if (renderingEnabled())
                    bg_next_tile_id = busRead(0x2000 | (vram_addr.reg & 0x0FFF));
            }
            if (prerender && cycle >= 280 && cycle < 305) TransferAddressY();

            if (visible && cycle == 64)
                sprite_overflow_cycle = renderingEnabled() ? computeSpriteOverflowCycle() : -1;
            if (visible && sprite_overflow_cycle >= 65 && cycle == sprite_overflow_cycle)
                status.sprite_overflow = 1;

            if (visible && cycle == 257 && renderingEnabled()) evaluateSprites();
            spriteFetchPhase();

            if (renderingEnabled() && cycle >= 257 && cycle <= 320) oamAddr = 0;
        }

        if (scanline == NES::SCANLINE_VBLANK_START && cycle == 1) {
            if (!suppressVblThisFrame) status.vertical_blank = 1;
            suppressVblThisFrame = false;
            ppuOpenBus = 0x00;
        }

        if (scanline == NES::SCANLINE_VBLANK_START && cycle == 0) nmiVbl = true;
        if (scanline == NES::SCANLINE_PRERENDER     && cycle == 0) nmiVbl = false;

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
            uint8_t nametable_x        : 1;
            uint8_t nametable_y        : 1;
            uint8_t increment_mode     : 1;
            uint8_t pattern_sprite     : 1;
            uint8_t pattern_background : 1;
            uint8_t sprite_size        : 1;
            uint8_t slave_mode         : 1;
            uint8_t enable_nmi         : 1;
        };
        uint8_t reg;
    } ctrl{};

    union PPUMASK {
        struct {
            uint8_t greyscale              : 1;
            uint8_t render_background_left : 1;
            uint8_t render_sprites_left    : 1;
            uint8_t render_background      : 1;
            uint8_t render_sprites         : 1;
            uint8_t enhance_red            : 1;
            uint8_t enhance_green          : 1;
            uint8_t enhance_blue           : 1;
        };
        uint8_t reg;
    } mask{};

    union PPUSTATUS {
        struct {
            uint8_t unused          : 5;
            uint8_t sprite_overflow : 1;
            uint8_t sprite_zero_hit : 1;
            uint8_t vertical_blank  : 1;
        };
        uint8_t reg;
    } status{};

    union loopy {
        struct {
            uint16_t coarse_x    : 5;
            uint16_t coarse_y    : 5;
            uint16_t nametable_x : 1;
            uint16_t nametable_y : 1;
            uint16_t fine_y      : 3;
            uint16_t unused      : 1;
        };
        uint16_t reg = 0;
    };
    loopy vram_addr;
    loopy tram_addr;

    uint8_t  fine_x          = 0;
    uint8_t  address_latch   = 0;
    uint8_t  ppu_data_buffer = 0;

    int16_t  scanline             = 0;
    int16_t  cycle                = 0;
    bool     frame_odd            = false;
    bool     suppressVblThisFrame = false;
    uint64_t totalPpuCycle        = 0; // TODO: can we get rid of it?
    bool     odd_frame_skip       = false;
    bool     nmiCycleLatch        = false;
    bool     nmiVbl               = false;
    int      completedFramesCount = 0;

    uint8_t  ppuOpenBus = 0x00;

    uint8_t  bg_next_tile_id       = 0;
    uint8_t  bg_next_tile_attrib   = 0;
    uint8_t  bg_next_tile_lsb      = 0;
    uint8_t  bg_next_tile_msb      = 0;
    uint16_t bg_shifter_pattern_lo = 0;
    uint16_t bg_shifter_pattern_hi = 0;
    uint16_t bg_shifter_attrib_lo  = 0;
    uint16_t bg_shifter_attrib_hi  = 0;

    uint8_t  OAM[256]{};
    uint8_t  oamAddr = 0;
    uint8_t  spriteScanline[8 * 4]{};
    uint8_t  sprite_count = 0;
    uint8_t  sprite_shifter_pattern_lo[8]{};
    uint8_t  sprite_shifter_pattern_hi[8]{};
    bool     bSpriteZeroHitPossible = false;
    int16_t  sprite_overflow_cycle  = -1;

    std::array<uint8_t, 32>          palScreen;
    std::array<uint32_t, 256 * 240>  buf;

    static uint16_t paletteIndex(uint16_t addr) {
        addr &= 0x001F;
        if (addr == 0x10 || addr == 0x14 || addr == 0x18 || addr == 0x1C) addr &= 0x000F;
        return addr;
    }

    bool renderingEnabled() const { return mask.render_background || mask.render_sprites; }

    void refreshOpenBus(uint8_t mask_bits, uint8_t value) {
        ppuOpenBus = (ppuOpenBus & ~mask_bits) | (value & mask_bits);
    }

    uint8_t readStatus(bool bReadOnly) {
        uint8_t ppu_bits = status.reg & 0xE0;
        uint8_t data = ppu_bits | (ppuOpenBus & 0x1F);
        if (!bReadOnly) {
            if (scanline == NES::SCANLINE_VBLANK_START) {
                if (cycle == 1) {
                    suppressVblThisFrame = true;
                }
                if (cycle == 1 || cycle == 2 || cycle == 3) {
                    nmiCycleLatch = false;
                }
            }
            status.vertical_blank = 0;
            nmiVbl = false;
            address_latch = 0;
            refreshOpenBus(0xE0, ppu_bits);
        }
        return data;
    }

    uint8_t readOAMData(bool bReadOnly) {
        if (renderingEnabled() && scanline <= NES::SCANLINE_VISIBLE_LAST
            && ((cycle >= 1 && cycle <= 64) || (cycle >= 257 && cycle <= 320))) {
            if (!bReadOnly) refreshOpenBus(0xFF, 0xFF);
            return 0xFF;
        }
        uint8_t data = OAM[oamAddr];
        if ((oamAddr & 0x03) == 0x02) data &= 0xE3;
        if (!bReadOnly) refreshOpenBus(0xFF, data);
        return data;
    }

    uint8_t readPPUData(bool bReadOnly) {
        uint8_t data;
        const uint16_t busAddr = vram_addr.reg & 0x3FFF;
        if (busAddr >= 0x3F00) {
            uint8_t pal = ppuRead(busAddr) & 0x3F;
            data = (ppuOpenBus & 0xC0) | pal;
            ppu_data_buffer = ppuRead(busAddr & 0x2FFF);
            if (!bReadOnly) refreshOpenBus(0x3F, pal);
        } else {
            data = ppu_data_buffer;
            ppu_data_buffer = ppuRead(busAddr);
            if (!bReadOnly) refreshOpenBus(0xFF, data);
        }
        if (!bReadOnly) {
            clockMapperA12();
            incrementVramAddr();
            clockMapperA12();
        }
        return data;
    }

    void backgroundFetchPhase() {
        if (!renderingEnabled()) return;
        if (!((cycle >= 2 && cycle < 258) || (cycle >= 321 && cycle < 338))) return;

        UpdateShifters();

        switch ((cycle - 1) & 7) {
        case 0:
            LoadBackgroundShifters();
            bg_next_tile_id = busRead(0x2000 | (vram_addr.reg & 0x0FFF));
            break;
        case 2: {
            uint16_t at = 0x23C0
                | (vram_addr.nametable_y << 11)
                | (vram_addr.nametable_x << 10)
                | ((vram_addr.coarse_y >> 2) << 3)
                | (vram_addr.coarse_x >> 2);
            bg_next_tile_attrib = busRead(at);
            if (vram_addr.coarse_y & 0x02) bg_next_tile_attrib >>= 4;
            if (vram_addr.coarse_x & 0x02) bg_next_tile_attrib >>= 2;
            bg_next_tile_attrib &= 0x03;
            break;
        }
        case 4: {
            const uint16_t base = ((uint16_t)ctrl.pattern_background << 12)
                + ((uint16_t)bg_next_tile_id << 4) + vram_addr.fine_y;
            bg_next_tile_lsb = busRead(base);
            break;
        }
        case 6: {
            const uint16_t base = ((uint16_t)ctrl.pattern_background << 12)
                + ((uint16_t)bg_next_tile_id << 4) + vram_addr.fine_y;
            bg_next_tile_msb = busRead(base + 8);
            break;
        }
        case 7:
            IncrementScrollX();
            break;
        default:
            break;
        }
    }

    void LoadBackgroundShifters() {
        bg_shifter_pattern_lo = (bg_shifter_pattern_lo & 0xFF00) | bg_next_tile_lsb;
        bg_shifter_pattern_hi = (bg_shifter_pattern_hi & 0xFF00) | bg_next_tile_msb;
        bg_shifter_attrib_lo  = (bg_shifter_attrib_lo  & 0xFF00) | ((bg_next_tile_attrib & 0x1) ? 0xFF : 0x00);
        bg_shifter_attrib_hi  = (bg_shifter_attrib_hi  & 0xFF00) | ((bg_next_tile_attrib & 0x2) ? 0xFF : 0x00);
    }

    void UpdateShifters() {
        if (renderingEnabled()) {
            bg_shifter_pattern_lo <<= 1;
            bg_shifter_pattern_hi = (bg_shifter_pattern_hi << 1) | 0x0001;
            bg_shifter_attrib_lo  <<= 1;
            bg_shifter_attrib_hi  <<= 1;
        }
        if (renderingEnabled() && cycle >= 1 && cycle < 258) {
            const bool visibleScanline = (scanline >= NES::SCANLINE_VISIBLE_FIRST
                                       && scanline <= NES::SCANLINE_VISIBLE_LAST);
            for (int i = 0; i < sprite_count; i++) {
                if (spriteScanline[i * 4 + 3] > 0)
                    spriteScanline[i * 4 + 3]--;
                else if (visibleScanline) {
                    sprite_shifter_pattern_lo[i] <<= 1;
                    sprite_shifter_pattern_hi[i] <<= 1;
                }
            }
        }
    }

    void IncrementScrollX() {
        if (!renderingEnabled()) return;
        if (vram_addr.coarse_x == 31) {
            vram_addr.coarse_x = 0;
            vram_addr.nametable_x = ~vram_addr.nametable_x;
        } else {
            vram_addr.coarse_x++;
        }
    }

    void IncrementScrollY() {
        if (!renderingEnabled()) return;
        if (vram_addr.fine_y < 7) {
            vram_addr.fine_y++;
        } else {
            vram_addr.fine_y = 0;
            if (vram_addr.coarse_y == 29) {
                vram_addr.coarse_y = 0;
                vram_addr.nametable_y = ~vram_addr.nametable_y;
            } else if (vram_addr.coarse_y == 31) {
                vram_addr.coarse_y = 0;
            } else {
                vram_addr.coarse_y++;
            }
        }
    }

    void TransferAddressX() {
        if (!renderingEnabled()) return;
        vram_addr.nametable_x = tram_addr.nametable_x;
        vram_addr.coarse_x    = tram_addr.coarse_x;
    }

    void TransferAddressY() {
        if (!renderingEnabled()) return;
        vram_addr.fine_y      = tram_addr.fine_y;
        vram_addr.nametable_y = tram_addr.nametable_y;
        vram_addr.coarse_y    = tram_addr.coarse_y;
    }

    void incrementVramAddr() {
        if (renderingEnabled() && (scanline == NES::SCANLINE_PRERENDER || scanline <= NES::SCANLINE_VISIBLE_LAST)) {
            IncrementScrollY();
            IncrementScrollX();
        } else {
            vram_addr.reg += (ctrl.increment_mode ? 32 : 1);
        }
    }

    int16_t computeSpriteOverflowCycle() const {
        const int spriteH = ctrl.sprite_size ? 16 : 8;
        int dot = 65, count = 0, m = 0;
        for (int n = 0; n < 64; n++) {
            if (dot > 256) break;
            if (count < 8) {
                const int16_t diff = (int16_t)scanline - (int16_t)OAM[n * 4];
                if (diff >= 0 && diff < spriteH) { count++; dot += 8; }
                else dot += 2;
            } else {
                const int16_t diff = (int16_t)scanline - (int16_t)OAM[n * 4 + m];
                if (diff >= 0 && diff < spriteH) return (int16_t)dot;
                m = (m + 1) & 3;
                dot += 2;
            }
        }
        return -1;
    }

    void evaluateSprites() {
        std::memset(spriteScanline, 0xFF, sizeof(spriteScanline));
        sprite_count = 0;
        std::memset(sprite_shifter_pattern_lo, 0, sizeof(sprite_shifter_pattern_lo));
        std::memset(sprite_shifter_pattern_hi, 0, sizeof(sprite_shifter_pattern_hi));
        bSpriteZeroHitPossible = false;

        const int spriteH = ctrl.sprite_size ? 16 : 8;
        uint8_t addr = oamAddr;
        for (int n = 0; n < 64; ++n, addr += 4) {
            int16_t diff = scanline - (int16_t)OAM[addr];
            if (diff < 0 || diff >= spriteH) continue;
            if (sprite_count < 8) {
                if (n == 0) bSpriteZeroHitPossible = true;
                for (int k = 0; k < 4; k++) {
                    uint8_t b = OAM[(uint8_t)(addr + k)];
                    if (((addr + k) & 0x03) == 0x02) b &= 0xE3;
                    spriteScanline[sprite_count * 4 + k] = b;
                }
                sprite_count++;
            }
        }
    }

    uint16_t spritePatternAddress(uint8_t tile, uint8_t attr, int row) const {
        const bool flipV = (attr & 0x80) != 0;
        if (!ctrl.sprite_size) {
            int r = flipV ? (7 - row) : row;
            return ((uint16_t)ctrl.pattern_sprite << 12) | ((uint16_t)tile << 4) | (uint16_t)(r & 0x07);
        }
        const uint16_t patTable = ((uint16_t)tile & 0x01) << 12;
        const uint16_t topTile  = (uint16_t)tile & 0xFE;
        int r = flipV ? (15 - row) : row;
        return patTable | (((r < 8) ? topTile : (topTile + 1)) << 4) | (uint16_t)(r & 0x07);
    }

    void loadSpritePatterns() {
        for (uint8_t i = 0; i < 8; i++) {
            uint8_t y    = (i < sprite_count) ? spriteScanline[i * 4 + 0] : 0xFF;
            uint8_t tile = (i < sprite_count) ? spriteScanline[i * 4 + 1] : 0xFF;
            uint8_t attr = (i < sprite_count) ? spriteScanline[i * 4 + 2] : 0xFF;
            uint16_t addrLo = spritePatternAddress(tile, attr, scanline - y);
            uint8_t lo = busRead(addrLo);
            uint8_t hi = busRead(addrLo + 8);
            if (i >= sprite_count) continue;
            if (attr & 0x40) { lo = flipByte(lo); hi = flipByte(hi); }
            sprite_shifter_pattern_lo[i] = lo;
            sprite_shifter_pattern_hi[i] = hi;
        }
    }

    void spriteFetchPhase() {
        if (!renderingEnabled()) return;
        if (cycle < 257 || cycle > 320) return;

        const bool visible = (scanline >= NES::SCANLINE_VISIBLE_FIRST && scanline <= NES::SCANLINE_VISIBLE_LAST);
        const int spriteIdx = (cycle - 257) / 8;
        const int phase     = (cycle - 257) & 7;

        if (phase == 0 || phase == 2) {
            busRead(0x2000 | (vram_addr.reg & 0x0FFF));
            return;
        }
        if (phase != 4 && phase != 6) return;

        const bool active = visible && spriteIdx < sprite_count;
        const uint8_t y    = active ? spriteScanline[spriteIdx * 4 + 0] : 0xFF;
        const uint8_t tile = active ? spriteScanline[spriteIdx * 4 + 1] : 0xFF;
        const uint8_t attr = active ? spriteScanline[spriteIdx * 4 + 2] : 0xFF;
        const uint16_t base = spritePatternAddress(tile, attr, scanline - y);
        const uint8_t data = busRead(base + (phase == 6 ? 8 : 0));
        if (!active) return;
        const uint8_t v = (attr & 0x40) ? flipByte(data) : data;
        if (phase == 4) sprite_shifter_pattern_lo[spriteIdx] = v;
        else            sprite_shifter_pattern_hi[spriteIdx] = v;
    }

    struct Pixel { uint8_t color; uint8_t palette; bool nonzero; };

    Pixel sampleBackground() const {
        if (!mask.render_background) return { 0, 0, false };
        if (!mask.render_background_left && cycle < 9) return { 0, 0, false };
        const uint16_t mux = 0x8000u >> fine_x;
        const uint8_t p0  = (bg_shifter_pattern_lo & mux) ? 1 : 0;
        const uint8_t p1  = (bg_shifter_pattern_hi & mux) ? 1 : 0;
        const uint8_t a0  = (bg_shifter_attrib_lo  & mux) ? 1 : 0;
        const uint8_t a1  = (bg_shifter_attrib_hi  & mux) ? 1 : 0;
        const auto col = (uint8_t)((p1 << 1) | p0);
        return { col, (uint8_t)((a1 << 1) | a0), col != 0 };
    }

    Pixel sampleSprite(bool& spriteZeroOut, bool& fgPriorityOut) {
        spriteZeroOut = false;
        fgPriorityOut = false;
        if (!mask.render_sprites) return { 0, 0, false };
        if (!mask.render_sprites_left && cycle < 9) return { 0, 0, false };
        for (uint8_t i = 0; i < sprite_count; i++) {
            if (spriteScanline[i * 4 + 3] != 0) continue;
            const uint8_t lo  = (sprite_shifter_pattern_lo[i] & 0x80) ? 1 : 0;
            const uint8_t hi  = (sprite_shifter_pattern_hi[i] & 0x80) ? 1 : 0;
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
        if ((unsigned)x >= (unsigned)NES::SCREEN_WIDTH || (unsigned)y >= (unsigned)NES::SCREEN_HEIGHT)
            return;

        Pixel bg = sampleBackground();
        bool spriteZero = false, fgPriority = false;
        Pixel sp = sampleSprite(spriteZero, fgPriority);
        const bool isSpriteZeroPixel = spriteZero && sp.nonzero;

        uint8_t pixel = 0, paletteIdx = 0;
        if (!bg.nonzero && sp.nonzero) {
            pixel = sp.color; paletteIdx = sp.palette;
        } else if (bg.nonzero && !sp.nonzero) {
            pixel = bg.color; paletteIdx = bg.palette;
        } else if (bg.nonzero && sp.nonzero) {
            if (fgPriority) { pixel = sp.color; paletteIdx = sp.palette; }
            else            { pixel = bg.color; paletteIdx = bg.palette; }
            if (bSpriteZeroHitPossible && isSpriteZeroPixel
                && mask.render_background && mask.render_sprites) {
                const int xLo = (mask.render_background_left && mask.render_sprites_left) ? 2 : 9;
                if (cycle >= xLo && cycle < 256) status.sprite_zero_hit = 1;
            }
        }

        uint8_t idx;
        if (!renderingEnabled() && (vram_addr.reg & 0x3F00) == 0x3F00)
            idx = palScreen[paletteIndex(vram_addr.reg)];
        else
            idx = palScreen[paletteIndex(0x3F00u | ((uint16_t)paletteIdx << 2) | pixel)];
        if (mask.greyscale) idx &= 0x30;
        const auto emph = (uint8_t)((mask.enhance_blue << 2) | (mask.enhance_green << 1) | mask.enhance_red);
        buf[y * NES::SCREEN_WIDTH + x] = emphasisLUT()[emph][idx & 0x3F];
    }

    void advanceCycle() {
        if (scanline == NES::SCANLINE_PRERENDER && cycle == (NES::PPU_CYCLES_PER_SCANLINE - 3))
            odd_frame_skip = frame_odd && renderingEnabled();

        cycle++;
        totalPpuCycle++;

        if (scanline == NES::SCANLINE_PRERENDER && cycle == (NES::PPU_CYCLES_PER_SCANLINE - 1) && odd_frame_skip) {
            cycle    = 0;
            scanline = NES::SCANLINE_VISIBLE_FIRST;
            odd_frame_skip = false;
            frame_odd = !frame_odd;
        } else if (cycle >= NES::PPU_CYCLES_PER_SCANLINE) {
            cycle = 0;
            if (scanline == NES::SCANLINE_VISIBLE_LAST) completedFramesCount++;
            scanline++;
            if (scanline >= NES::TOTAL_SCANLINES) {
                scanline  = NES::SCANLINE_VISIBLE_FIRST;
                frame_odd = !frame_odd;
            }
        }
    }

    static const std::array<uint32_t, 64>& palette() {
        static const std::array lut = {
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
        cart.clockA12(addr, totalPpuCycle);
        return ppuRead(addr);
    }

    void clockMapperA12() {
        cart.clockA12(vram_addr.reg & 0x3FFF, totalPpuCycle);
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
            (unsigned)vram_addr.reg, (unsigned)tram_addr.reg,
            (unsigned)fine_x, (unsigned)address_latch,
            (unsigned)ctrl.reg, (unsigned)mask.reg, (unsigned)status.reg,
            (unsigned)oamAddr, (unsigned)sprite_count,
            nmiLineLow() ? 1u : 0u,
            frame_odd ? " ODD" : "");
        tracer->writePpu(str);
    }
};
