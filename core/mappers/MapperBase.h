#pragma once

#include <cstdint>

// Typy mirroringu (kluczowe dla renderowania t�a przez PPU)
enum class Mirroring {
	HORIZONTAL,
	VERTICAL,
	ONESCREEN_LO,
	ONESCREEN_HI,
	FOURSCREEN,
};

// ============================================================================
// Helpery mappingu - wspolne dla wielu mapperow.
// ----------------------------------------------------------------------------
// Wiele mapperow powtarza dokladnie ten sam wzorzec mapowania PRG/CHR. Te
// inline'owe helpery pozwalaja zwinac go do jednej linii w mapperze i
// jednoczesnie ujednolicaja maskowanie nr banku (zawsze przez prgBanks/chrBanks),
// co eliminuje off-by-one na ROM-ach mniejszych niz oczekuje mapper.
//
// Konwencja:
//   - prgBanks liczone w jednostkach 16 KB (jak iNES header)
//   - chrBanks liczone w jednostkach 8 KB
// ============================================================================
namespace mapper_helpers {

// Maska indeksu banku do liczby dostepnych. Dla potegi 2 to AND;
// dla nie-potegi modulo (bardzo rzadki przypadek w iNES).
inline uint8_t maskBank(uint8_t bank, uint8_t numBanks) {
	if (numBanks == 0) return 0;
	if ((numBanks & (numBanks - 1)) == 0) return bank & (numBanks - 1); // pot. 2
	return bank % numBanks;
}

// --- PRG ---

// 16 KB switchable @ $8000-$BFFF + 16 KB fixed @ $C000-$FFFF (UxROM-like).
inline uint32_t mapPrg16k_fixedHi(uint16_t addr, uint8_t bankLo, uint8_t prgBanks) {
	if (addr < 0xC000)
		return (uint32_t)maskBank(bankLo, prgBanks) * 0x4000 + (addr & 0x3FFF);
	return (uint32_t)maskBank(prgBanks - 1, prgBanks) * 0x4000 + (addr & 0x3FFF);
}

// 16 KB switchable @ $C000-$FFFF + 16 KB fixed @ $8000-$BFFF (mniej popularne, np. M180).
inline uint32_t mapPrg16k_fixedLo(uint16_t addr, uint8_t bankHi, uint8_t prgBanks) {
	if (addr < 0xC000)
		return (uint32_t)0 * 0x4000 + (addr & 0x3FFF);
	return (uint32_t)maskBank(bankHi, prgBanks) * 0x4000 + (addr & 0x3FFF);
}

// 32 KB switchable @ $8000-$FFFF (NROM-256, GxROM, Color Dreams, AxROM).
// 'bank32' liczony w jednostkach 32 KB => maksymalna wartosc = prgBanks/2 - 1.
inline uint32_t mapPrg32k(uint16_t addr, uint8_t bank32, uint8_t prgBanks) {
	uint8_t n = prgBanks / 2;
	if (n == 0) n = 1;
	return (uint32_t)maskBank(bank32, n) * 0x8000 + (addr & 0x7FFF);
}

// --- CHR ---

// 8 KB switchable @ $0000-$1FFF (CNROM, GxROM, Bandai i klony).
// chrBanks==0 => CHR-RAM, ignoruj numer banku, mapuj 1:1.
inline uint32_t mapChr8k(uint16_t addr, uint8_t bank, uint8_t chrBanks) {
	if (chrBanks == 0) return addr & 0x1FFF;
	return (uint32_t)maskBank(bank, chrBanks) * 0x2000 + (addr & 0x1FFF);
}

// Standardowy fallback zapisu do CHR: pozwala zapisac tylko jesli ROM ma CHR-RAM
// (czyli chrBanks == 0 i rozmiar CHR == 8 KB). Zwraca true gdy mapowanie 1:1 jest OK.
inline bool chrRamWrite(uint16_t addr, uint32_t& mapped, uint8_t chrBanks) {
	if (addr <= 0x1FFF && chrBanks == 0) { mapped = addr; return true; }
	return false;
}

} // namespace mapper_helpers

// ============================================================================
// Mapper (klasa bazowa)
// ----------------------------------------------------------------------------
// Ka�dy mapper t�umaczy adres widziany przez CPU (0x6000-0xFFFF) lub PPU
// (0x0000-0x1FFF) na konkretne przesuni�cie w ROM-ach PRG / CHR kartrid�a.
// Funkcje cpuMapRead/Write/ppuMapRead/Write zwracaj� true gdy zaadresowa�y
// pami�� i wype�ni�y 'mapped' warto�ci� offsetu w odpowiednim wektorze.
// ============================================================================
class Mapper {
public:
	Mapper(uint8_t prgBanks, uint8_t chrBanks) : prgBanks(prgBanks), chrBanks(chrBanks) {}
	virtual ~Mapper() = default;

	virtual bool cpuMapRead(uint16_t addr, uint32_t& mapped, uint8_t& data) = 0;
	virtual void cpuMapWrite(uint16_t addr, uint32_t& mapped, uint8_t data) = 0;
	virtual bool ppuMapRead(uint16_t addr, uint32_t& mapped) = 0;
	virtual bool ppuMapWrite(uint16_t addr, uint32_t& mapped) = 0;

	virtual void reset() {}

	// Mapper mo�e dynamicznie zmienia� mirroring (np. MMC1, AxROM, MMC3)
	virtual Mirroring mirror() const { return Mirroring::HORIZONTAL; }
	virtual bool hasDynamicMirror() const { return false; }

	// Skanlinia / IRQ � u�ywane przez mappery z wewn�trznym licznikiem (MMC3 itp.)
	virtual void scanline() {}
	virtual bool irqState() const { return false; }
	virtual void irqClear() {}

	// Sygna� A12 z PPU (rosn�ce zbocze) � u�ywane przez precyzyjne IRQ MMC3.
	// Domy�lnie deleguje do scanline() przy rosn�cym zboczu, co daje rozs�dny
	// wynik dop�ki PPU nie zacznie wywo�ywa� tej funkcji bezpo�rednio.
	virtual void clockA12(bool /*a12High*/) {}

	// Zegar CPU � u�ywane przez VRC IRQ (mappery 21�26, 73) � pojedynczy
	// cykl CPU. Domy�lnie no-op.
	virtual void clock() {}

	// Wyjscie ekspansyjnego audio mappera (VRC6, VRC7, MMC5, FME-7, N163, ...).
	// Domyslnie cisza. Wartosc jest w przyblizonej skali wyjscia 2A03 (~0..1).
	// Sumowane z probka APU w NESSystem.
	virtual float audioOutput() const { return 0.0f; }

	// Niekt�re proste mappery (CNROM, AxROM, GxROM, BNROM/NINA, kilka klon�w)
	// cierpi� na bus conflicts � warto�� zapisywana w obszar ROM jest zAND-owana
	// z bajtem aktualnie wystawianym przez ROM. Cartridge obs�uguje to za nas,
	// mapper wystawia tylko flag�.
	virtual bool hasBusConflicts() const { return false; }

protected:
	uint8_t prgBanks;
	uint8_t chrBanks;
};
