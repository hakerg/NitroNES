# NES Emulator

Emulator NES robiony pod kątem minimalnych opóźnień oraz zgodności z fizycznym sprzętem. Cel: zaliczenie wszystkich testów.

## Zasady pisania kodu

- Architektura kodu musi być zgodna z architekturą sprzętu (komponent = klasa).
- Unikaj dodawania zmiennych do klas, chyba że to konieczne — im mniej stanu, tym mniejsze ryzyko błędów.
- Dbaj o spójność i czytelność kodu. Jeśli nazewnictwo wprowadza zamieszanie, popraw je.
- Nie dodawaj komentarzy — dobry kod sam się komentuje.
- Im mniej kodu i im prostszy, tym lepszy.
- Do budowania używaj ninja.
- W `nes-test-roms-master/AccuracyCoin-main/AccuracyCoin.asm` są cenne obszerne komentarze o testach.
- Możesz odpalać inne ROM-y testowe przez `nes_test`, aby pozyskać więcej informacji.
- Dokumentacja różnych komponentów jest rozbita na wiele plików, więc pamiętaj, żeby sprawdzić wszystkie, bo każdy zawiera cenne informacje. Oszczędzi to wiele czasu przy analizie.
- Aktualny stan testów: uruchom `python run_tests.py` (buduje projekt, odpala accuracy_coin i wszystkie ROM-y testowe, wypisuje faile).

## Ważne katalogi

- `src/` - kod źródłowy emulatora
- `nes_specs/` - specyfikacja NES. Może być niekompletna - jeśli czegoś brakuje, poinformuj, żeby uzupełnić.
- `nes-test-roms-master/` - ROM-y testowe; przy większości jest kod źródłowy z komentarzami oraz readme z oczekiwanymi wynikami.
- `test/` - kod narzędzi diagnostycznych (patrz niżej).
- `MesenCE/` - kod źródłowy emulatora Mesen (oraz jego plik wykonywalny).
- `metalnes/` - kod źródłowy emulatora zgodnego na poziomie tranzystorów (nie odpala się na Windows)

## Narzędzia diagnostyczne

### run_tests.py (przegląd stanu — preferowany do sprawdzania)
Buduje cały projekt przez ninja, odpala accuracy_coin oraz
wszystkie ROM-y testowe z jednoznacznym wynikiem tekstowym (testy wizualne
i niejednoznaczne są pomijane — nie da się ich zweryfikować automatycznie)
i wypisuje aktualny stan: wszystkie faile z powodami. Nic nie porównuje —
raport to po prostu stan. Użycie:
- `python run_tests.py` — build + wszystkie testy (~2 min, w tym Holy Mapperel)
- `python run_tests.py --no-build` — bez przebudowy

ROM-y są odpalane w trybie interaktywnym nes_test (stdin): pętla
`frames:30` + `ascii`, klasyfikacja co krok, wczesny stop przy wyniku
(2 kolejne poll-e z tym samym statusem dla pewności; Holy Mapperel po
pierwszym). Testy z resetem (cpu_reset, apu_reset) używają klasycznego
przebiegu jednorazowego.

Aktualne faile i ich powody (stan na dziś):
- `mmc3_test/5-MMC3` i `mmc3_irq_tests/6.MMC3_rev_B` — testy rewizji B
  (Sharp); emulujemy rewizję A/MMC6, rewizje są wzajemnie wykluczające się
- `read_joy3/count_errors*` i `test_buttons` to INFO (szum sprzętu / interaktywne)
- Holy Mapperel: patrz sekcja poniżej (fail = ekran z detałem != 0000 albo kod morsa)

### holy_mapperel (nes-test-roms-master/holy-mapperel/)
Test płytki PCB autorstwa Damiana Yerricka (pinobatch/holy-mapperel): wykrywa
mapper po mirroringu, testuje bankowanie PRG/CHR, WRAM, CHR RAM i IRQ.
Wynik wyświetla na ekranie albo — przy twardej awarii — pika kodem morsa.
ROM-y generowane z repo (repo nie zawiera gotowych ROM-ów):
```
cd nes-test-roms-master/holy-mapperel
export PATH="<repo>/cc65-snapshot-win32/bin:$PATH"   # ca65/ld65
make            # buduje mapperel-primary.nes (wymaga ca65, Pillow, git tag)
cd tools && py make_roms.py   # generuje 35 ROM-ów do ../testroms/
```
Pliki `.lbl` przy ROM-ach (kopie primary.lbl) dają symbole w trace CPU.

Mapa znaków dla `ascii:` (font 8x5: litery na kafelkach $01-$1A, reszta jak ASCII):
```
ascii:' ABCDEFGHIJKLMNOPQRSTUVWXYZ      !"#$%&'()*+,-./0123456789:;<=>?'
```

Kody morsa (README + morse.inc): WB = wrong bank przy starcie, MIR = mirroring
nie pasuje do żadnego mappera, SU = SUROM 4M, LB/RB = zły bank po detekcji/
teście, CBT = CHR bank tags niespójne, FON = font w CHR nie zgadza się z PRG,
DRV = brak sterownika mappera.

Ekran: `DETAILED TEST RESULT: XXXX` — 4 cyfry (WRAM, PRG, IRQ, CHR), 0 = OK.
Znane kody: MMC1 `1xxx` = $E000 bit 4 nie wyłącza WRAM; MMC1 `4xxx` = $A000
bit 4; MMC3 `2xxx` = brak trybu read-only WRAM. Linie `... OK / PROBLEM /
MISSING` dla PRG RAM i CHR.

run_tests.py klasyfikuje automatycznie: ekran z detałem != 0000 lub
PROBLEM/MISSING = fail (z pełnym opisem). Brak ekranu wyniku = detekcja kodu
morsa z pamięci, bez trace: litery morsa to stałe w kodzie ROM (wzorzec
`A9 xx 20 B0 FF` = LDA #lit; JSR morsebeep), a JSR wpycha na stos
(adres_powrotu - 1) — run_tests.py skanuje stos CPU ($01F0, `mem:`) i zeropage
($01-$03 dla ścieżki MIR), mapując wciśnięte adresy na litery (`hm_morse_sites()`
buduje mapę z pliku ROM). M69 (FME-7) = fail "emulator nie implementuje tego
mappera" (Cartridge odrzuca ROM). MISSING na ekranie = fail tylko gdy nagłówek
ROM-u deklaruje PRG RAM (bajt 10 nagłówka), inaczej oczekiwane.

### accuracy_coin (build/release/accuracy_coin.exe)
Odpala `nes-test-roms-master/AccuracyCoin-main/AccuracyCoin.nes` i zwraca listę testów z wynikami pass/fail.

### nes_test (build/release/nes_test.exe)
Skryptowalny harness — zamiast hard-coded logiki dostaje listę komend.
Akceptowane wejście:
- `*.nes` — uruchamia od razu
- `*.nsf` — uruchamia przez rdzeń NSFPlayer (headless `NESHeadlessNSF`), brak
  PPU/ekranu; komenda `frames:N` odpowiada N wywołaniom `play()`.
- `*.asm` — kompiluje przez nesasm3 w katalogu pliku źródłowego; build
  nadpisuje istniejące artefakty w tym katalogu. Obok wynikowego `.nes`
  pojawia się `.fns`.
- `*.s`  — kompiluje przez ca65 + linkuje przez ld65 (z `--feature force_range`
  dla zgodności z kodem blargga); artefakty (`.o`, `.nes`, `.lbl`) trafiają do
  katalogu pliku źródłowego, nadpisując istniejące.
  `nes.cfg` i katalog `common/` są wyszukiwane automatycznie obok pliku
  źródłowego (w górę drzewa do 4 poziomów).

Symbole z `.fns` (nesasm) lub `.lbl` w formacie VICE (ld65) są ładowane
automatycznie z miejsca obok wynikowego `.nes` i pojawiają się w trace CPU.

Użycie:
  nes_test <rom-or-asm-or-nsf> [command]...

Tryb interaktywny (stdin): bez komend w argv nes_test czyta komendy ze
standardowego wejścia — jedna na linię (puste linie i `#` są ignorowane,
`quit`/`exit` kończy sesję). Po każdej komendzie stdout jest flushowany,
więc skrypt może sterować krok po kroku: `frames:1`, potem `ascii`, sprawdzić
wynik i albo kontynuować, albo ubić proces. Na tym opiera się run_tests.py
(`run_rom_poll` — pętla frames:30 + ascii, wczesny stop przy wyniku).
Przykład (potok):
  printf 'frames:120\nascii:MAP\nquit\n' | nes_test rom.nes

Komendy (wykonywane sekwencyjnie, każda jako osobny argv lub linia stdin):
  frames:N             clockuj N klatek (lub N wywołań play() dla .nsf)
  cycles:N             clockuj N cykli CPU
  reset                soft reset
  screen               wypisz nametable 0 (32x30) jako indeksy kafelków w hex
                       (np. 24 24 0C 19...)
  ascii[:MAP]          wypisz nametable 0 (32x30) jako znaki.
                       MAP to ciąg znaków, gdzie pozycja N = znak dla kafelka
                       o indeksie N. Nie musi mieć pełnych 256 znaków — brakujące
                       kafelki renderowane są jako spacja.
                       Domyślnie (bez MAP): mapowanie standardowego ASCII
                       (kafelek N = znak ASCII N dla $20-$7E).
                       AccuracyCoin: patrz zalecana mapa poniżej.
  pixels:X:Y:W:H       zrzuć wycinek framebuffera WxH od (X,Y) jako RGB w hex,
                       jedna linia na rząd (np. FF0000 00FF00 0000FF)
  mem:ADDR:LEN         dump LEN bajtów z magistrali CPU od ADDR (hex/dec/$hex/0xhex)
  info                 wypisz aktualny frame count i cycle count
  pad1=BTNS            ustaw stan pada 1 (BTNS = lista po przecinku, puste = wyzeruj)
  pad1+BTN[,BTN..]     naciśnij wskazane przyciski (set bit)
  pad1-BTN[,BTN..]     puść wskazane przyciski (clear bit)
  pad2=... +... -...   to samo dla pada 2
  trace:CHAN:STATE     włącz/wyłącz kanał trace (CHAN=cpu|ppu|dma|apu, STATE=on|off)
  trace-file:PATH      zmień plik wyjściowy trace (domyślnie: trace.log)
  ac:PAGE:ROW          nawiguj menu AccuracyCoin do strony PAGE (1-based),
                       wiersza ROW (1-based), przez pad RIGHT/DOWN (20-frame timing),
                       a następnie naciska A, uruchamiając podświetlony test.
  song:N               (tylko .nsf) initSong(N)
  next / prev          (tylko .nsf) przełącz na następny/poprzedni utwór
  songinfo             (tylko .nsf) wypisz nazwę/artystę/copyright/load/init/play

Przyciski: A, B, SELECT, START, UP, DOWN, LEFT, RIGHT

Przykłady (liczbę klatek można dostosować, testy mogą działać od 1s do 30s):
  nes_test mytest.asm frames:600 screen mem:0x00:16
  nes_test mytest.asm trace:cpu:on trace:dma:on trace:apu:on frames:5
  nes_test ROM.nes frames:120 pad1+START frames:1 pad1-START frames:600 ascii
  nes_test AccuracyCoin.asm frames:600 ascii:'0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ .!?:,      +-*/=$'
  nes_test ROM.nes frames:120 pixels:0:0:16:8
  nes_test ROM.nes frames:600 reset frames:600 mem:0x6000:32
  nes_test song.nsf songinfo cycles:5000000 trace:cpu:on cycles:100000 info
  nes_test AccuracyCoin.asm ac:18:2 frames:600 trace:cpu:on trace:ppu:on
  nes_test AccuracyCoin.asm ac:14:6 frames:600 trace:cpu:on trace:dma:on trace:apu:on

### Zalecana mapa znaków dla AccuracyCoin

AccuracyCoin używa własnego fontu (tablica `AsciiToCHR` w `AccuracyCoin.asm`).
Odwzorowanie indeks kafelka → znak:

```
$00-$09: 0123456789
$0A-$23: ABCDEFGHIJKLMNOPQRSTUVWXYZ
$24:     (spacja)
$25:     .
$26:     !
$27:     ?
$28:     :
$29:     ,
$2A:     *
$2B:     +
$2C:     (spacja, nieużywane)
$2D:     -
$2E:     (spacja, nieużywane)
$2F:     /
$30:     +
$31:     -
$32:     *
$33:     /
$34:     =
$35:     $
```

Tekst zaznaczony kursorem (inwersja) ma ustawiony bit $80 w indeksie kafelka
(np. $8A = 'A' w inwersji). Te kafelki ($80+) mają osobne wzory w CHR i nie
są pokryte powyższą mapą — w `ascii` wyrenderują się jako spacja, chyba że
mapa zostanie rozszerzona o pozycje $80+.

Rozszerzona mapa (zaznaczone znaki = indeks + $80):
```
nes_test AccuracyCoin.asm frames:600 ascii:'0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ .!?:,      +-*/=$                                                                          0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ .!?:,      +-*/=$'
```

### Uruchamianie pojedynczego testu AccuracyCoin z trace

`accuracy_coin.exe` automatycznie naciska START i odpala wszystkie testy
naraz — nie da się wtedy włączyć trace tylko dla jednego (bo `trace.log` by
spuchł do gigabajtów). Zamiast tego buduj `AccuracyCoin.asm` przez `nes_test`
i użyj komendy `ac:PAGE:ROW`, która nawiguje menu do wskazanego testu i
naciska A, uruchamiając go — cały proces w jednej komendzie. Trace włącza się
komendą `trace:...` po `ac:...`, dzięki czemu tylko wybrany test ląduje
w log-u. Po `ac:...` trzeba poczekać komendą `frames:N` na zakończenie testu.

Menu AccuracyCoin:
- Strony testów (suity) wybiera się **RIGHT/LEFT** (kolumna)
- Pojedynczy test wybiera się **DOWN/UP** (wiersz)
- **A** uruchamia podświetlony test
- Suity są zdefiniowane w `AccuracyCoin.asm` jako etykiety `Suite_*`
  (kolejność = numer strony, kolejność `table ...` w suicie = numer wiersza)

Między każdym wciśnięciem a puszczeniem przycisku musi upłynąć ok. **20 klatek**,
żeby gra zdążyła przejść między ekranami (jedno wciśnięcie = pad1+X, frames:20,
pad1-X, frames:20). Komenda `ac` dba o to automatycznie.

Symbole `.fns` (generowane automatycznie przy buildzie `.asm`) ujawniają nazwę
funkcji testu w trace — np. `TEST_DeltaModulationChannel`,
`TEST_ExplicitDMAAbort`, `TEST_APULengthCounter`. Dzięki temu od razu wiadomo
który fragment AccuracyCoin.asm robi co — można porównać krok po kroku
sekwencję dostępów do magistrali z kodem źródłowym testu.

### Szablon ASM (test/nes_template.asm)
Minimalny ROM gotowy do wypełnienia kodem testowym.
Krytyczne zasady formatu NESASM3:
- Wszystkie dyrektywy (.inesprg, .bank, .org, .word) MUSZĄ mieć wcięcie TAB
- .inesprg 2 = 32KB = 4 banki po 8KB (bank 0=$8000, 1=$A000, 2=$C000, 3=$E000)
- Kod testowy: .bank 2 / .org $C000
- Wektory: .bank 3 / .org $FFFA / .word nmi / .word reset / .word irq
- Przed VBlank looopem: wyłącz APU IRQ: lda #$40 / sta $4017
