#pragma once
#include <cstdint>
#include <array>
#include <cstring>
#include <functional>
#include "PPUBus.h"
#include "NESConst.h"

class PPU2C02 {
public:

    PPU2C02() {
        buf.fill(0xFF000000);
        // Power-up zawartosc palety - wartosci zmierzone na fizycznym NES
        // (test blargg/power_up_palette). NES nie zeruje palety przy resecie;
        // zawiera ona "smieci" o powtarzalnym wzorze zaleznym od konstrukcji.
        static constexpr uint8_t kPowerUpPalette[32] = {
            0x09,0x01,0x00,0x01, 0x00,0x02,0x02,0x0D,
            0x08,0x10,0x08,0x24, 0x00,0x00,0x04,0x2C,
            0x09,0x01,0x34,0x03, 0x00,0x04,0x00,0x14,
            0x08,0x3A,0x00,0x02, 0x00,0x20,0x2C,0x08,
        };
        std::memcpy(palScreen.data(), kPowerUpPalette, sizeof(kPowerUpPalette));
        std::memset(OAM, 0, sizeof(OAM));
        std::memset(spriteScanline, 0xFF, sizeof(spriteScanline));
    }

    PPUBus* ppuBus = nullptr;
    std::function<void()> onFrameComplete;

    bool nmiLineLow() const { return ctrl.enable_nmi && status.vertical_blank; }

    // ====================================================
    //  Interfejs CPU - rejestry $2000-$2007 (mirror co 8)
    // ====================================================
    void cpuWrite(uint16_t addr, uint8_t data) {
        // Kazdy zapis do $2000-$2007 wstawia bajt do open-bus PPU i odswieza
        // licznik decay dla wszystkich 8 bitow (ppu_open_bus test #2).
        refreshOpenBus(0xFF, data);
        switch (addr & 0x0007) {
        case 0: // PPUCTRL
            ctrl.reg = data;
            tram_addr.nametable_x = ctrl.nametable_x;
            tram_addr.nametable_y = ctrl.nametable_y;
            break;
        case 1: // PPUMASK
        {
            mask.reg = data;
            updateEmphasisCache();
        }
        break;
        case 2: break; // PPUSTATUS - tylko do odczytu
        case 3: oamAddr = data; break;
        case 4: OAM[oamAddr++] = data; break;
        case 5: // PPUSCROLL
            if (!address_latch) {
                fine_x = data & 0x07;
                fine_x_mux = 0x8000u >> fine_x;
                tram_addr.coarse_x = data >> 3;
                address_latch = 1;
            }
            else {
                tram_addr.fine_y = data & 0x07;
                tram_addr.coarse_y = data >> 3;
                address_latch = 0;
            }
            break;
        case 6: // PPUADDR
            if (!address_latch) {
                tram_addr.reg = (uint16_t)(((data & 0x3F) << 8) | (tram_addr.reg & 0x00FF));
                address_latch = 1;
            }
            else {
                tram_addr.reg = (tram_addr.reg & 0xFF00) | data;
                vram_addr.reg = tram_addr.reg;
                address_latch = 0;
            }
            break;
        case 7: // PPUDATA
            ppuWrite(vram_addr.reg, data);
            vram_addr.reg += (ctrl.increment_mode ? 32 : 1);
            break;
        }
    }

    uint8_t cpuRead(uint16_t addr, bool bReadOnly = false) {
        switch (addr & 0x0007) {
        case 0: case 1: case 3: case 5: case 6:
            return ppuOpenBus; // write-only porty -> caly bajt = open bus
        case 2: return readStatus(bReadOnly);
        case 4: return readOAMData(bReadOnly);
        case 7: return readPPUData(bReadOnly);
        }
        return ppuOpenBus;
    }

    // OAM DMA $4014 wykonywane przez bus
    void oamDMAWrite(uint8_t data) { OAM[oamAddr++] = data; }
    uint8_t getOamAddr() const { return oamAddr; }

    // ====================================================
    //  Wewnetrzna pamiec PPU (PPUBus + paleta)
    // ====================================================
    uint8_t ppuRead(uint16_t addr) {
        addr &= 0x3FFF;
        if (addr >= 0x3F00) {
            uint8_t v = palScreen[paletteIndex(addr)];
            return mask.greyscale ? (v & 0x30) : v;
        }
        return ppuBus ? ppuBus->read(addr) : 0x00;
    }

    void ppuWrite(uint16_t addr, uint8_t data) {
        addr &= 0x3FFF;
        if (addr >= 0x3F00) {
            palScreen[paletteIndex(addr)] = data;
            return;
        }
        if (ppuBus) ppuBus->write(addr, data);
    }

    void reset() {
        fine_x = address_latch = ppu_data_buffer = 0;
        scanline = NES::SCANLINE_PRERENDER;
        cycle = 0;
        bg_next_tile_id = bg_next_tile_attrib = 0;
        bg_next_tile_lsb = bg_next_tile_msb = 0;
        bg_shifter_pattern_lo = bg_shifter_pattern_hi = 0;
        bg_shifter_attrib_lo = bg_shifter_attrib_hi = 0;
        status.reg = mask.reg = ctrl.reg = 0;
        vram_addr.reg = tram_addr.reg = 0;
        oamAddr = 0;
        frame_odd = false;
        last_a12 = false;
        odd_frame_skip = false;
        sprite_count = 0;
        bSpriteZeroHitPossible = bSpriteZeroBeingRendered = false;
        suppressVblThisFrame = false;
        ppuOpenBus = 0x00;
        for (int i = 0; i < 8; ++i) ppuOpenBusDecay[i] = 0;
        std::memset(spriteScanline, 0xFF, sizeof(spriteScanline));
        std::memset(sprite_shifter_pattern_lo, 0, sizeof(sprite_shifter_pattern_lo));
        std::memset(sprite_shifter_pattern_hi, 0, sizeof(sprite_shifter_pattern_hi));
    }

    const uint32_t* getFramebuffer() const {
        return buf.data();
    }

    // ====================================================
    //  CLOCK - pojedynczy dot PPU. Wolany 3x na cykl CPU.
    // ====================================================
    void clock() {
        const bool visible = (scanline >= NES::SCANLINE_VISIBLE_FIRST && scanline <= NES::SCANLINE_VISIBLE_LAST);
        const bool prerender = (scanline == NES::SCANLINE_PRERENDER);
        const bool rendering = visible || prerender;

        if (rendering) {
            if (prerender && cycle == 1) {
                status.vertical_blank = 0;
                status.sprite_zero_hit = 0;
                status.sprite_overflow = 0;
                std::memset(sprite_shifter_pattern_lo, 0, sizeof(sprite_shifter_pattern_lo));
                std::memset(sprite_shifter_pattern_hi, 0, sizeof(sprite_shifter_pattern_hi));
            }

            backgroundFetchPhase();

            if (cycle == 256) IncrementScrollY();
            if (cycle == 257) { LoadBackgroundShifters(); TransferAddressX(); }
            if (cycle == 338 || cycle == 340) {
                bg_next_tile_id = busRead(0x2000 | (vram_addr.reg & 0x0FFF));
            }
            if (prerender && cycle >= 280 && cycle < 305) TransferAddressY();

            // Uproszczone fazy sprite: evaluation cycle=257, pattern fetch cycle=340.
            if (visible && cycle == 257) evaluateSprites();
            if (visible && cycle == 340) loadSpritePatterns();
        }

        // VBL flag set (dot 1 linii 241).
        if (scanline == NES::SCANLINE_VBLANK_START && cycle == 1) {
            if (!suppressVblThisFrame) status.vertical_blank = 1;
            suppressVblThisFrame = false;
        }

        renderPixel();

        // MMC3 scanline counter (bezpieczna alternatywa do A12 - hook co linia).
        if (renderingEnabled() && cycle == 260 && scanline <= NES::SCANLINE_VISIBLE_LAST) {
            if (ppuBus && ppuBus->cart) ppuBus->cart->scanline();
        }

        advanceCycle();
    }

    // Biezacy scanline PPU (-1 = pre-render, 0-239 = visible, 240-260 = vblank).
    int16_t getScanline() const { return scanline; }

private:
    // ----------------------------------------------------------
    //  Rejestry
    // ----------------------------------------------------------
    union PPUCTRL {
        struct {
            uint8_t nametable_x : 1;
            uint8_t nametable_y : 1;
            uint8_t increment_mode : 1;
            uint8_t pattern_sprite : 1;
            uint8_t pattern_background : 1;
            uint8_t sprite_size : 1;
            uint8_t slave_mode : 1;
            uint8_t enable_nmi : 1;
        };
        uint8_t reg;
    } ctrl{};

    union PPUMASK {
        struct {
            uint8_t greyscale : 1;
            uint8_t render_background_left : 1;
            uint8_t render_sprites_left : 1;
            uint8_t render_background : 1;
            uint8_t render_sprites : 1;
            uint8_t enhance_red : 1;
            uint8_t enhance_green : 1;
            uint8_t enhance_blue : 1;
        };
        uint8_t reg;
    } mask{};

    union PPUSTATUS {
        struct {
            uint8_t unused : 5;
            uint8_t sprite_overflow : 1;
            uint8_t sprite_zero_hit : 1;
            uint8_t vertical_blank : 1;
        };
        uint8_t reg;
    } status{};

    union loopy {
        struct {
            uint16_t coarse_x : 5;
            uint16_t coarse_y : 5;
            uint16_t nametable_x : 1;
            uint16_t nametable_y : 1;
            uint16_t fine_y : 3;
            uint16_t unused : 1;
        };
        uint16_t reg = 0;
    };
    loopy vram_addr;
    loopy tram_addr;

    uint8_t  fine_x = 0;
    uint8_t  address_latch = 0;
    uint8_t  ppu_data_buffer = 0;

    int16_t  scanline = 0;
    int16_t  cycle = 0;
    bool     frame_odd = false;
    bool     last_a12 = false;
    bool     suppressVblThisFrame = false;
    bool     odd_frame_skip = false;

    // ---- Open-bus / decay register ----------------------------------
    uint8_t  ppuOpenBus = 0x00;
    uint8_t  ppuOpenBusDecay[8]{};
    static constexpr uint8_t OPEN_BUS_DECAY_FRAMES = 36; // ~600 ms @ 60 Hz

    // ---- Background pipeline ----------------------------------------
    uint8_t  bg_next_tile_id = 0;
    uint8_t  bg_next_tile_attrib = 0;
    uint8_t  bg_next_tile_lsb = 0;
    uint8_t  bg_next_tile_msb = 0;
    uint16_t bg_shifter_pattern_lo = 0;
    uint16_t bg_shifter_pattern_hi = 0;
    uint16_t bg_shifter_attrib_lo = 0;
    uint16_t bg_shifter_attrib_hi = 0;

    // ---- OAM / sprite pipeline --------------------------------------
    uint8_t  OAM[256]{};
    uint8_t  oamAddr = 0;
    uint8_t  spriteScanline[8 * 4]{};
    uint8_t  sprite_count = 0;
    uint8_t  sprite_shifter_pattern_lo[8]{};
    uint8_t  sprite_shifter_pattern_hi[8]{};
    bool     bSpriteZeroHitPossible = false;
    bool     bSpriteZeroBeingRendered = false;

    // ---- Paleta / framebuffer ---------------------------------------
    std::array<uint8_t, 32>          palScreen;
    std::array<uint32_t, 256 * 240>  buf;

    // Zakeszowana maska przesuwajaca dla fine_x (0x8000 >> fine_x).
    uint16_t fine_x_mux = 0x8000;

    // Zakeszowany indeks emphasis (bity 5-7 PPUMASK). Aktualizowany przy
    // kazdym zapisie PPUMASK - unika rekonstrukcji co piksel w renderPixel().
    uint8_t  emphasis_index = 0;

    // ----------------------------------------------------------
    //  Helpery
    // ----------------------------------------------------------
    static uint16_t paletteIndex(uint16_t addr) {
        addr &= 0x001F;
        if (addr == 0x10 || addr == 0x14 || addr == 0x18 || addr == 0x1C) addr &= 0x000F;
        return addr;
    }

    bool renderingEnabled() const { return mask.render_background || mask.render_sprites; }

    void refreshOpenBus(uint8_t mask_bits, uint8_t value) {
        ppuOpenBus = (ppuOpenBus & ~mask_bits) | (value & mask_bits);
        for (int i = 0; i < 8; ++i)
            if (mask_bits & (1 << i)) ppuOpenBusDecay[i] = OPEN_BUS_DECAY_FRAMES;
    }

    void decayOpenBus() {
        for (int i = 0; i < 8; ++i)
            if (ppuOpenBusDecay[i] > 0 && --ppuOpenBusDecay[i] == 0)
                ppuOpenBus &= ~(uint8_t)(1 << i);
    }

    void updateEmphasisCache() {
        emphasis_index = (uint8_t)((mask.enhance_red ? 1 : 0)
                                 | (mask.enhance_green ? 2 : 0)
                                 | (mask.enhance_blue  ? 4 : 0));
    }

    // ----------------------------------------------------------
    //  CPU register read helpers
    // ----------------------------------------------------------
    uint8_t readStatus(bool bReadOnly) {
        uint8_t ppu_bits = (status.reg & 0xE0);
        uint8_t data = ppu_bits | (ppuOpenBus & 0x1F);
        if (!bReadOnly) {
            // VBL race / NMI suppression: odczyt $2002 na dot 1 linii 241
            // (przed setem VBL) tlumi VBL i NMI dla tej klatki.
            if (scanline == NES::SCANLINE_VBLANK_START && cycle == 1) {
                suppressVblThisFrame = true;
                data &= 0x7F;
            }
            status.vertical_blank = 0;
            address_latch = 0;
            refreshOpenBus(0xE0, ppu_bits);
        }
        return data;
    }

    uint8_t readOAMData(bool bReadOnly) {
        // Bity 2..4 atrybutu sprite (offset 2 w krotce OAM) zawsze czytaja 0.
        uint8_t data = OAM[oamAddr];
        if ((oamAddr & 0x03) == 0x02) data &= 0xE3;
        if (!bReadOnly) refreshOpenBus(0xFF, data);
        return data;
    }

    uint8_t readPPUData(bool bReadOnly) {
        uint8_t data;
        // Detekcja palety operuje na 14-bitowym adresie wystawianym na PPU bus
        // (v jest 15-bitowe i moze przyjmowac $4000-$7FFF po inkrementacji
        // z $3FFF; wtedy adres na busie to v & 0x3FFF, czyli NIE paleta).
        const uint16_t busAddr = vram_addr.reg & 0x3FFF;
        if (busAddr >= 0x3F00) {
            // Paleta: bity 5..0 zdefiniowane, 7..6 to open bus.
            uint8_t pal = ppuRead(busAddr) & 0x3F;
            data = (ppuOpenBus & 0xC0) | pal;
            // Bufor odczytu jest rownolegle ladowany z PPU bus pod adresem
            // pod paleta - linie adresowe wystawiaja vram_addr na bus, a
            // paleta jest tylko wewnetrzna, wiec bus zwraca nametable mirror
            // (addr & 0x2FFF).
            ppu_data_buffer = ppuBus ? ppuBus->read(busAddr & 0x2FFF) : 0x00;
            if (!bReadOnly) refreshOpenBus(0x3F, pal);
        }
        else {
            data = ppu_data_buffer;
            ppu_data_buffer = ppuRead(busAddr);
            if (!bReadOnly) refreshOpenBus(0xFF, data);
        }
        if (!bReadOnly)
            vram_addr.reg += (ctrl.increment_mode ? 32 : 1);
        return data;
    }

    // ----------------------------------------------------------
    //  Background fetch / shifter pipeline
    // ----------------------------------------------------------
    void backgroundFetchPhase() {
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
        }
    }

    void LoadBackgroundShifters() {
        bg_shifter_pattern_lo = (bg_shifter_pattern_lo & 0xFF00) | bg_next_tile_lsb;
        bg_shifter_pattern_hi = (bg_shifter_pattern_hi & 0xFF00) | bg_next_tile_msb;
        bg_shifter_attrib_lo = (bg_shifter_attrib_lo & 0xFF00) | ((bg_next_tile_attrib & 0x1) ? 0xFF : 0x00);
        bg_shifter_attrib_hi = (bg_shifter_attrib_hi & 0xFF00) | ((bg_next_tile_attrib & 0x2) ? 0xFF : 0x00);
    }

    void UpdateShifters() {
        if (mask.render_background) {
            bg_shifter_pattern_lo <<= 1;
            bg_shifter_pattern_hi <<= 1;
            bg_shifter_attrib_lo <<= 1;
            bg_shifter_attrib_hi <<= 1;
        }
        if (mask.render_sprites && cycle >= 1 && cycle < 258) {
            for (int i = 0; i < sprite_count; i++) {
                if (spriteScanline[i * 4 + 3] > 0) {
                    spriteScanline[i * 4 + 3]--;
                }
                else {
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
        }
        else vram_addr.coarse_x++;
    }

    void IncrementScrollY() {
        if (!renderingEnabled()) return;
        if (vram_addr.fine_y < 7) {
            vram_addr.fine_y++;
        }
        else {
            vram_addr.fine_y = 0;
            if (vram_addr.coarse_y == 29) {
                vram_addr.coarse_y = 0;
                vram_addr.nametable_y = ~vram_addr.nametable_y;
            }
            else if (vram_addr.coarse_y == 31) {
                vram_addr.coarse_y = 0;
            }
            else {
                vram_addr.coarse_y++;
            }
        }
    }

    void TransferAddressX() {
        if (!renderingEnabled()) return;
        vram_addr.nametable_x = tram_addr.nametable_x;
        vram_addr.coarse_x = tram_addr.coarse_x;
    }

    void TransferAddressY() {
        if (!renderingEnabled()) return;
        vram_addr.fine_y = tram_addr.fine_y;
        vram_addr.nametable_y = tram_addr.nametable_y;
        vram_addr.coarse_y = tram_addr.coarse_y;
    }

    // ----------------------------------------------------------
    //  Sprite evaluation / fetch
    // ----------------------------------------------------------
    void evaluateSprites() {
        std::memset(spriteScanline, 0xFF, sizeof(spriteScanline));
        sprite_count = 0;
        std::memset(sprite_shifter_pattern_lo, 0, sizeof(sprite_shifter_pattern_lo));
        std::memset(sprite_shifter_pattern_hi, 0, sizeof(sprite_shifter_pattern_hi));
        bSpriteZeroHitPossible = false;

        const int spriteH = ctrl.sprite_size ? 16 : 8;

        for (uint8_t n = 0; n < 64; ++n) {
            int16_t diff = (int16_t)scanline - (int16_t)OAM[n * 4 + 0];
            if (diff < 0 || diff >= spriteH) continue;

            if (sprite_count < 8) {
                if (n == 0) bSpriteZeroHitPossible = true;
                std::memcpy(&spriteScanline[sprite_count * 4], &OAM[n * 4], 4);
                sprite_count++;
            }
            else {
                status.sprite_overflow = 1;
                break;
            }
        }
    }

    uint16_t spritePatternAddress(uint8_t tile, uint8_t attr, int row) const {
        const bool flipV = (attr & 0x80) != 0;
        if (!ctrl.sprite_size) {
            int r = flipV ? (7 - row) : row;
            return ((uint16_t)ctrl.pattern_sprite << 12)
                | ((uint16_t)tile << 4)
                | (uint16_t)(r & 0x07);
        }
        // 8x16
        const uint16_t patTable = ((uint16_t)tile & 0x01) << 12;
        const uint16_t topTile = (uint16_t)tile & 0xFE;
        int r = flipV ? (15 - row) : row;
        const uint16_t tileSel = (r < 8) ? topTile : (topTile + 1);
        return patTable | (tileSel << 4) | (uint16_t)(r & 0x07);
    }

    void loadSpritePatterns() {
        for (uint8_t i = 0; i < sprite_count; i++) {
            uint8_t y = spriteScanline[i * 4 + 0];
            uint8_t tile = spriteScanline[i * 4 + 1];
            uint8_t attr = spriteScanline[i * 4 + 2];

            uint16_t addrLo = spritePatternAddress(tile, attr, scanline - y);
            uint16_t addrHi = addrLo + 8;
            uint8_t  lo = busRead(addrLo);
            uint8_t  hi = busRead(addrHi);

            if (attr & 0x40) { // flip horizontal
                lo = flipByte(lo);
                hi = flipByte(hi);
            }
            sprite_shifter_pattern_lo[i] = lo;
            sprite_shifter_pattern_hi[i] = hi;
        }
    }

    // ----------------------------------------------------------
    //  Pixel composition / sprite-0 hit
    // ----------------------------------------------------------
    struct Pixel { uint8_t color; uint8_t palette; bool nonzero; };

    Pixel sampleBackground() const {
        if (!mask.render_background) return { 0, 0, false };
        if (!mask.render_background_left && cycle < 9) return { 0, 0, false };

        const uint8_t p0 = (bg_shifter_pattern_lo & fine_x_mux) ? 1 : 0;
        const uint8_t p1 = (bg_shifter_pattern_hi & fine_x_mux) ? 1 : 0;
        const uint8_t a0 = (bg_shifter_attrib_lo & fine_x_mux) ? 1 : 0;
        const uint8_t a1 = (bg_shifter_attrib_hi & fine_x_mux) ? 1 : 0;
        const uint8_t col = (uint8_t)((p1 << 1) | p0);
        return { col, (uint8_t)((a1 << 1) | a0), col != 0 };
    }

    Pixel sampleSprite(bool& spriteZeroOut, bool& fgPriorityOut) {
        spriteZeroOut = false;
        fgPriorityOut = false;
        if (!mask.render_sprites) return { 0, 0, false };
        if (!mask.render_sprites_left && cycle < 9) return { 0, 0, false };

        for (uint8_t i = 0; i < sprite_count; i++) {
            if (spriteScanline[i * 4 + 3] != 0) continue; // X-counter jeszcze nie zeskoczyl do 0

            const uint8_t lo = (sprite_shifter_pattern_lo[i] & 0x80) ? 1 : 0;
            const uint8_t hi = (sprite_shifter_pattern_hi[i] & 0x80) ? 1 : 0;
            const uint8_t col = (uint8_t)((hi << 1) | lo);
            if (col == 0) continue;

            const uint8_t attr = spriteScanline[i * 4 + 2];
            const uint8_t pal = (attr & 0x03) + 0x04;
            fgPriorityOut = (attr & 0x20) == 0;
            spriteZeroOut = (i == 0);
            return { col, pal, true };
        }
        return { 0, 0, false };
    }

    void renderPixel() {
        const int x = cycle - 1;
        const int y = scanline;

        // Background/sprite shifters dostarczaja danych tylko dla widocznych
        // pikseli (cycle 1..256, scanline 0..239). Sprite-0 hit jest setowany
        // tylko gdy cycle < 256, wiec poza tym oknem renderowanie to no-op.
        if ((unsigned)x >= (unsigned)NES::SCREEN_WIDTH || (unsigned)y >= (unsigned)NES::SCREEN_HEIGHT) {
            return;
        }

        Pixel bg = sampleBackground();
        bool spriteZero = false, fgPriority = false;
        Pixel sp = sampleSprite(spriteZero, fgPriority);
        bSpriteZeroBeingRendered = spriteZero && sp.nonzero;

        uint8_t pixel = 0, paletteIdx = 0;
        if (!bg.nonzero && sp.nonzero) { pixel = sp.color; paletteIdx = sp.palette; }
        else if (bg.nonzero && !sp.nonzero) { pixel = bg.color; paletteIdx = bg.palette; }
        else if (bg.nonzero && sp.nonzero) {
            if (fgPriority) { pixel = sp.color; paletteIdx = sp.palette; }
            else { pixel = bg.color; paletteIdx = bg.palette; }

            // ----- Sprite 0 hit (docs/ppu.txt: "Sprite 0 hits") -----
            // Warunki: oba piksele non-transparent, sprite 0 renderowany,
            // bity BG i sprites = 1, x != 255 (cycle != 256), oraz dla x w 0..7
            // OBA bity left-clip musza byc wlaczone.
            if (bSpriteZeroHitPossible && bSpriteZeroBeingRendered
                && mask.render_background && mask.render_sprites)
            {
                // Sprite 0 hit "acts as if the image starts at cycle 2"
                // (docs/PPU rendering.txt: Cycles 1-256). Earliest possible
                // hit dot = 2; ostatni piksel (x=255 -> cycle=256) wykluczony.
                const int xLo = (mask.render_background_left && mask.render_sprites_left) ? 2 : 9;
                if (cycle >= xLo && cycle < 256) {
                    status.sprite_zero_hit = 1;
                }
            }
        }

        // Backdrop override (docs/PPU rendering.txt: "Rendering disabled"):
        // gdy rendering wylaczony i v wskazuje w $3F00-$3FFF, PPU rysuje
        // kolor z tego adresu zamiast standardowego backdropu $3F00.
        uint8_t idx;
        if (!renderingEnabled() && (vram_addr.reg & 0x3F00) == 0x3F00) {
            idx = palScreen[paletteIndex(vram_addr.reg)];
        } else {
            idx = palScreen[paletteIndex(0x3F00u | ((uint16_t)paletteIdx << 2) | pixel)];
        }
        if (mask.greyscale) idx &= 0x30;
        buf[y * NES::SCREEN_WIDTH + x] = emphasisLUT()[emphasis_index][idx & 0x3F];
    }

    void advanceCycle() {
        // Zatrzaskujemy warunek pominięcia klatki na końcu cyklu 338.
        // Symuluje to sprzętowe opóźnienie (setup time/propagation delay)
        // dla bitów maski docierających do układu ucinającego cykl 340.
        if (scanline == NES::SCANLINE_PRERENDER && cycle == (NES::PPU_CYCLES_PER_SCANLINE - 3)) {
            odd_frame_skip = frame_odd && renderingEnabled();
        }

        cycle++;

        // Wykonujemy pominięcie na takcie 340, korzystając z zatrzaśniętej wartości
        if (scanline == NES::SCANLINE_PRERENDER && cycle == (NES::PPU_CYCLES_PER_SCANLINE - 1) && odd_frame_skip) {
            cycle = 0;
            scanline = NES::SCANLINE_VISIBLE_FIRST;
            odd_frame_skip = false;
        }
        else if (cycle >= NES::PPU_CYCLES_PER_SCANLINE) {
            cycle = 0;
            if (scanline == NES::SCANLINE_VISIBLE_LAST) { if (onFrameComplete) onFrameComplete(); }
            scanline++;

            if (scanline >= NES::TOTAL_SCANLINES) {
                scanline = NES::SCANLINE_VISIBLE_FIRST;
                frame_odd = !frame_odd;
                decayOpenBus();
            }
        }
    }

    // ----------------------------------------------------------
    //  Paleta NES (FBX Smooth) - 64 wpisy ARGB8888.
    // ----------------------------------------------------------
    static const std::array<uint32_t, 64>& palette() {
        static const std::array<uint32_t, 64> lut = {
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

    // Precomputed LUT: emphasisLUT[emph3bit][nesColorIdx] -> ARGB.
    // emph3bit = (enhance_blue<<2)|(enhance_green<<1)|enhance_red (bity 5-7 PPUMASK).
    // Budowany raz, niezmienialny przez caly czas zycia obiektu.
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
                        uint32_t cb =  argb        & 0xFF;
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

    // Bity 5-7 PPUMASK (R/G/B emphasis) tlumia dwa pozostale kanaly o ~25%.
    // Greyscale (bit 0) jest juz aplikowany w ppuRead() na samym indeksie.
    uint32_t applyEmphasis(uint32_t argb) const {
        if (!(mask.enhance_red || mask.enhance_green || mask.enhance_blue))
            return argb;
        uint32_t r = (argb >> 16) & 0xFF;
        uint32_t g = (argb >> 8) & 0xFF;
        uint32_t b = argb & 0xFF;
        // Kanaly NIE wzmocnione tlumimy do ~75%.
        if (!mask.enhance_red)   r = (r * 3) >> 2;
        if (!mask.enhance_green) g = (g * 3) >> 2;
        if (!mask.enhance_blue)  b = (b * 3) >> 2;
        return (argb & 0xFF000000u) | (r << 16) | (g << 8) | b;
    }

    static uint8_t flipByte(uint8_t b) {
        b = (uint8_t)(((b & 0xF0) >> 4) | ((b & 0x0F) << 4));
        b = (uint8_t)(((b & 0xCC) >> 2) | ((b & 0x33) << 2));
        b = (uint8_t)(((b & 0xAA) >> 1) | ((b & 0x55) << 1));
        return b;
    }

    uint8_t busRead(uint16_t addr) {
        addr &= 0x3FFF;
        const bool a12 = (addr & 0x1000) != 0;
        if (a12 && !last_a12 && ppuBus && ppuBus->cart) {
            ppuBus->cart->clockA12(true);
        }
        last_a12 = a12;
        return ppuRead(addr);
    }
};