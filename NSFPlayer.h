#pragma once
#include <cstdint>
#include <array>
#include <vector>
#include <string>
#include <iostream>
#include <memory>
#include <filesystem>

#include "NESConst.h"
#include "NESCoreBase.h"
#include "NSFLoader.h"
#include "mappers/MapperBase.h"
#include "mappers/MapperRegistry.h"
#include "mappers/AllMappers.h"

class NSFPlayer : public NESCoreBase {
public:
	static constexpr uint16_t TRAMPOLINE_ADDR = 0x5000; // Adres pulapki (JMP $5000)
	static constexpr uint16_t RESET_VECTOR    = 0xFFFC;

	NSFPlayer() {
		extRam.fill(0x00);
		prgRom.assign(32768, 0x00);
	}

	bool load(const NSFFile& nsf) {
		nsfHeader = nsf.header;  // kopiujemy tylko 128 B naglowka
		pal = nsfIsPAL() && !nsfIsDualMode();
		cpuClock    = pal ? NES::CPU_CLOCK_PAL   : NES::CPU_CLOCK_NTSC;
		frameClocks = pal ? NES::CPU_CYCLES_PER_FRAME_PAL : NES::CPU_CYCLES_PER_FRAME_NTSC;
		playCycles  = calcPlayCycles(pal);

		// Skonfiguruj timing APU pod region (tabele okresow + frame counter)
		apu.setPAL(pal);

		// Expansion audio chip przez interfejs Mapper.
		expChip.reset();
		uint8_t chips = nsfHeader.extraChipFlags;
		if (chips & 0x01) {
			expChip = MapperRegistry::instance().create(24, 1, 1); // VRC6
		}

		// Zaladuj dane NSF do PRG ROM
		if (isBankswitched()) {
			loadBankswitched(nsf.data);
		} else {
			uint16_t base   = nsfHeader.loadAddr;
			size_t   maxLen = prgRom.size() - (base - 0x8000);
			size_t   len    = std::min(nsf.data.size(), maxLen);
			std::fill(prgRom.begin(), prgRom.end(), 0x00);
			std::copy(nsf.data.begin(), nsf.data.begin() + len,
					  prgRom.begin() + (base - 0x8000));
		}

		// Trampoline pod TRAMPOLINE_ADDR: JMP $5000
		trampoline[0] = 0x4C;                          // JMP abs
		trampoline[1] = TRAMPOLINE_ADDR & 0xFF;
		trampoline[2] = (TRAMPOLINE_ADDR >> 8) & 0xFF;

		currentSong = nsfHeader.startingSong;
		initSong(currentSong);
		return true;
	}

	// Zainicjalizuj i odtworz dany utwor (1-based)
	void initSong(uint8_t songNum) {
		currentSong = songNum;
		cycleCount  = 0;
		playTimer   = 0;
		callDone    = true;

		// Spec: "Clear all RAM at 0000h-07FFh and 6000h-7FFFh"
		cpuRam.fill(0x00);
		extRam.fill(0x00);

		// Spec: Init APU registers (NESdev NSF init sequence)
		for (uint16_t a = 0x4000; a <= 0x400F; a++) apu.cpuWrite(a, 0x00);
		apu.cpuWrite(0x4010, 0x10);
		apu.cpuWrite(0x4011, 0x00);
		apu.cpuWrite(0x4012, 0x00);
		apu.cpuWrite(0x4013, 0x00);
		apu.cpuWrite(NES::APU_STATUS_ADDR, 0x0F);
		// $4017 <- $40: 4-step bez frame IRQ (NSF nie ma handlera $FFFE).
		apu.cpuWrite(NES::APU_FRAME_CTR_ADDR, 0x40);

		// Bankswitch - zaladuj wartosci startowe
		if (isBankswitched()) {
			for (int i = 0; i < 8; i++)
				banks[i] = nsfHeader.bankValues[i];
		}

		// Ustaw wektor reset na trampoline
		cpuRam[RESET_VECTOR & 0x07FF] = 0x00; // nie uzywany bezposrednio

		// Ustaw rejestry CPU i wywolaj INIT
		// A = numer utworu (0-based), X = 0 (NTSC) lub 1 (PAL)
		cpu.reset();
		cpu.A = songNum - 1;
		cpu.X = pal ? 1 : 0;
		cpu.P = FLAG_I | FLAG_U;
		cpu.S = 0xFD;

		// JSR do INIT via trampoline: ustawiamy PC na INIT, push adres powrotu
		pushWord(TRAMPOLINE_ADDR - 1);
		cpu.PC = nsfHeader.initAddr;

		// Wykonaj INIT do momentu powrotu (max 200000 cykli)
		int initCycles = 0;
		for (; initCycles < 200000 && cpu.PC != TRAMPOLINE_ADDR; initCycles++)
			cpu.tick();

		if (cpu.PC != TRAMPOLINE_ADDR) {
			// INIT nie wrocil w limicie - kontynuujemy mimo to.
		}

	}

	void nextSong() {
		uint8_t next = currentSong + 1;
		if (next > nsfHeader.totalSongs) next = 1;
		initSong(next);
	}

	void prevSong() {
		uint8_t prev = currentSong - 1;
		if (prev < 1) prev = nsfHeader.totalSongs;
		initSong(prev);
	}

	uint8_t getCurrentSong()  const { return currentSong; }
	uint8_t getTotalSongs()   const { return nsfHeader.totalSongs; }

	// --- NESCoreBase API --------------------------------------------------
	bool loadFile(const std::string& path) override {
		NSFFile nsf;
		if (!NSFLoader::load(path, nsf)) return false;
		return load(nsf);
	}

	std::string windowTitle(const std::string& /*filename*/) const override {
		return std::string("NSF Player - ") + nsfName() + " | "
			 + std::string(nsfHeader.artist, ::strnlen(nsfHeader.artist, 32));
	}
	void defaultWindowSize(int& w, int& h) const override { w = 480; h = 120; }

	void onSpacePressed() override { togglePause(); }
	void onRightPressed() override { changeSong(+1); }
	void onLeftPressed()  override { changeSong(-1); }

private:
	// Zmiana utworu musi byc atomowa wzgledem producer-callback - inaczej
	// trafilibysmy w polowe initSong().
	void changeSong(int delta) {
		if (delta >= 0) nextSong(); else prevSong();
		if (resetAudio) resetAudio();
	}

public:
	// Jeden cykl CPU w trybie NSF. Brak PPU, brak NMI. Linia /IRQ
	// zawsze nieaktywna (NSF V1.61: brak wektorow przerwan).
	void clockOneCycle() override {
		trampolineMaintenance();
		if (expChip) expChip->clock();
		apu.clock();
		cpu.setIRQ(false);
		cpu.tick();

		cycleCount++;
		if (playTimer > 0) playTimer--;

		if (!callDone && cpu.PC == TRAMPOLINE_ADDR) {
			callDone = true;
		}

		if (apu.dmcNeedsSample()) {
			uint8_t s = cpuRead(apu.dmcSampleAddress());
			apu.loadDMCSample(s);
			cpu.addStall(4);
		}
	}

	float getAudioSample() override {
		return apu.getOutputSample() + (expChip ? expChip->audioOutput() : 0.0f);
	}

private:
	// Trampoline pod $5000: zarzadzanie wywolaniem PLAY.
	void trampolineMaintenance() {
		if (callDone && playTimer == 0) {
			// PC/SP wolno modyfikowac TYLKO na granicy instrukcji - inaczej
			// trampoline JMP $5000 (3 cykle) doczyta high byte z playAddr+1.
			if (!cpu.isAtInstructionBoundary()) return;

			// Twardy reset SP - wiele ripow konczy PLAY inna sciezka niz
			// RTS (np. JMP indirect), bez tego stos drenuje sie po ~127
			// wywolaniach i nadpisuje silnik muzyczny w $0100-$01FF.
			cpu.S = 0xFD;
			callDone = false;
			pushWord(TRAMPOLINE_ADDR - 1);
			cpu.PC = nsfHeader.playAddr;
			playTimer = playCycles;
			playRunCycles = 0;
		} else if (!callDone) {
			// Watchdog: PLAY ktore nie wrocilo w 4x czas miedzy wywolaniami
			// forsujemy z powrotem na trampoline.
			if (++playRunCycles > playCycles * 4u && playCycles > 0) {
				if (!cpu.isAtInstructionBoundary()) return;
				cpu.PC = TRAMPOLINE_ADDR;
				playRunCycles = 0;
			}
		}
	}

	// --- Helpery naglowka NSF ---------------------------------------------
	bool isBankswitched() const {
		for (int i = 0; i < 8; i++)
			if (nsfHeader.bankValues[i] != 0) return true;
		return false;
	}
	bool nsfIsPAL()      const { return (nsfHeader.palNtscBits & 0x01) != 0; }
	bool nsfIsDualMode() const { return (nsfHeader.palNtscBits & 0x02) != 0; }
	std::string nsfName() const {
		int len = 0;
		while (len < 32 && nsfHeader.songName[len] != '\0') len++;
		return std::string(nsfHeader.songName, len);
	}
	uint32_t calcPlayCycles(bool palMode) const {
		uint32_t clk   = palMode ? NES::CPU_CLOCK_PAL  : NES::CPU_CLOCK_NTSC;
		uint16_t speed = palMode ? nsfHeader.speedPAL   : nsfHeader.speedNTSC;
		if (speed == 0) speed = palMode ? NES::NSF_SPEED_PAL : NES::NSF_SPEED_NTSC;
		return (uint32_t)((uint64_t)clk * speed / 1000000);
	}

	// --- Pamiec CPU --------------------------------------------------------
	uint8_t cpuRead(uint16_t addr) override {
		if (addr <= 0x07FF) return cpuRam[addr];
		// PPU stub: NSF rip czesto czeka na VBlank (LDA $2002 / BPL -).
		// Zwracamy 0x80 zeby symulowac stale ustawiona flage VBlank.
		if (addr >= 0x2000 && addr <= 0x3FFF) return 0x80;
		if (addr >= 0x4000 && addr <= 0x4017) return apu.cpuRead(addr);
		if (addr >= 0x5000 && addr <= 0x5002) return trampoline[addr - 0x5000];
		if (addr >= 0x6000 && addr <= 0x7FFF) return extRam[addr - 0x6000];
		if (addr >= 0x8000) {
			if (isBankswitched()) {
				uint8_t  bankIdx = (addr - 0x8000) / 4096;
				uint16_t offset  = (addr - 0x8000) % 4096;
				uint32_t romAddr = (uint32_t)banks[bankIdx] * 4096 + offset;
				if (romAddr < bankRom.size()) return bankRom[romAddr];
				return 0x00;
			}
			return prgRom[addr - 0x8000];
		}
		return 0x00;
	}

	void cpuWrite(uint16_t addr, uint8_t data) override {
		if (addr <= 0x07FF) { cpuRam[addr] = data; return; }
		if (addr >= 0x2000 && addr <= 0x3FFF) return; // PPU stub
		if (addr >= 0x4000 && addr <= 0x4017) { apu.cpuWrite(addr, data); return; }
		if (addr >= 0x6000 && addr <= 0x7FFF) { extRam[addr - 0x6000] = data; return; }
		if (addr >= NES::NSF_BANK_BASE && addr <= 0x5FFF) {
			banks[addr - NES::NSF_BANK_BASE] = data;
			return;
		}
		if (expChip && addr >= 0x8000) {
			uint32_t dummy = 0;
			expChip->cpuMapWrite(addr, dummy, data);
		}
	}

	void loadBankswitched(const std::vector<uint8_t>& data) {
		uint16_t loadOffset  = nsfHeader.loadAddr & 0x0FFF;
		size_t   romCapacity = 256 * 4096; // 1MB
		bankRom.assign(romCapacity, 0x00);
		size_t len = std::min(data.size(), romCapacity - loadOffset);
		std::copy(data.begin(), data.begin() + len,
				  bankRom.begin() + loadOffset);
		for (int i = 0; i < 8; i++)
			banks[i] = nsfHeader.bankValues[i];
	}

	void pushWord(uint16_t val) {
		cpuRam[0x0100 + cpu.S] = (val >> 8) & 0xFF;
		cpu.S--;
		cpuRam[0x0100 + cpu.S] = val & 0xFF;
		cpu.S--;
	}

	// Tylko naglowek (128 B) - dane muzyczne sa kopiowane raz do prgRom/bankRom podczas load()
	NSFHeader nsfHeader  = {};
	uint32_t  cpuClock    = NES::CPU_CLOCK_NTSC;
	uint32_t  frameClocks = NES::CPU_CYCLES_PER_FRAME_NTSC;
	uint32_t  playCycles  = 0;
	uint32_t  cycleCount  = 0;
	uint32_t  playTimer   = 0;
	uint32_t  playRunCycles = 0;  // ile cykli aktualne PLAY juz sie wykonuje
	bool      callDone    = true;
	uint8_t   currentSong = 1;


	std::array<uint8_t, 8192>  extRam;    // 0x6000-0x7FFF
	std::vector<uint8_t>       prgRom;    // 0x8000-0xFFFF (32 KB, na stercie)

	// Bankswitch: 8 bankow x 4KB = 32KB przestrzeni adresowej
	std::array<uint8_t, 8>   banks   = {};
	std::vector<uint8_t>     bankRom; // Maks. 1MB danych NSF w bankach

	uint8_t trampoline[3] = {};  // JMP $5000

	std::unique_ptr<Mapper> expChip; // expansion audio (VRC6 itp.)
};
