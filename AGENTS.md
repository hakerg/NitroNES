# NES Emulator

Emulator NES robiony pod kątem zgodności z fizycznym sprzętem. Cel: zaliczenie wszystkich testów AccuracyCoin.

## Zasady pisania kodu

- Architektura kodu musi być zgodna z architekturą sprzętu (komponent = klasa).
- Unikaj dodawania zmiennych do klas, chyba że to konieczne — im więcej stanu, tym większe ryzyko rozjazdów.
- Dbaj o spójność i czytelność kodu. Jeśli nazewnictwo wprowadza zamieszanie, popraw je.
- Nie dodawaj komentarzy — dobry kod sam się komentuje.
- Im mniej kodu i im prostszy, tym lepszy.
- Do budowania używaj ninja.

## Ważne katalogi

- `src/` - kod źródłowy emulatora (core, sdl, lang)
- `nes_specs/` - cała specyfikacja NES, źródło prawdy. Może być niekompletna - jeśli czegoś brakuje, poinformuj, żeby uzupełnić.
- `nes-test-roms-master/` - ROM-y testowe; przy większości jest kod źródłowy z komentarzami oraz readme z oczekiwanymi wynikami.
- `test/` - kod narzędzi diagnostycznych (patrz niżej).

## Zalecany workflow diagnostyczny

Gdy test accuracy_coin nie przechodzi:
1. Przeczytaj opis błędu i kod źródłowy testu w `nes-test-roms-master/AccuracyCoin-main/AccuracyCoin.asm`
2. Napisz minimalny plik .asm odtwarzający konkretne scenario (szablon: `test/nes_template.asm`)
3. Uruchom `nes_test` z odpowiednimi komendami, włącz trace cpu/ppu/dma/apu — zapis trafi do `trace.log`
4. Przeanalizuj trace, symbole z `.fns` pojawiają się automatycznie
5. Porównaj z oczekiwanym zachowaniem ze specyfikacji w `nes_specs/`
6. Możesz też odpalać inne ROM-y testowe przez `nes_test`, aby pozyskać więcej informacji

Dokumentacja różnych komponentów jest rozbita na wiele plików, więc pamiętaj, żeby sprawdzić wszystkie, bo każdy zawiera cenne informacje. Oszczędzi to wiele czasu przy analizie.
Aktualny stan testów: zbuduj `accuracy_coin` przez ninja, a następnie uruchom `build/release/accuracy_coin.exe`.

## Narzędzia diagnostyczne

### accuracy_coin (build/release/accuracy_coin.exe)
Odpala AccuracyCoin.nes i zwraca listę testów z wynikami pass/fail.

### nes_test (build/release/nes_test.exe)
Skryptowalny harness — zamiast hard-coded logiki dostaje listę komend.
Akceptowane wejście:
- `*.nes` — uruchamia od razu
- `*.nsf` — uruchamia przez rdzeń NSFPlayer (headless `NESHeadlessNSF`), brak
  PPU/ekranu; komenda `frames:N` odpowiada N wywołaniom `play()`.
- `*.asm` — kompiluje przez nesasm3. Zawartość katalogu źródłowego jest
  kopiowana do `%TEMP%/nes_test_build/<hash>/<basename>/` i build leci tam —
  repo nigdy nie jest modyfikowane. Obok wynikowego `.nes` pojawia się `.fns`.
- `*.s`  — kompiluje przez ca65 + linkuje przez ld65 (z `--feature force_range`
  dla zgodności z kodem blargga); artefakty trafiają do
  `%TEMP%/nes_test_build/<hash>/<basename>/` — repo nigdy nie jest dotykane.
  `nes.cfg` i katalog `common/` są wyszukiwane automatycznie obok pliku
  źródłowego (w górę drzewa do 4 poziomów).

Symbole z `.fns` (nesasm) lub `.lbl` w formacie VICE (ld65) są ładowane
automatycznie z miejsca obok wynikowego `.nes` i pojawiają się w trace CPU.

Użycie:
  nes_test <rom-or-asm-or-nsf> [command]...

Komendy (wykonywane sekwencyjnie, każda jako osobny argv):
  frames:N             clockuj N klatek PPU (lub N wywołań play() dla .nsf)
  cycles:N             clockuj N cykli CPU
  reset                hard reset
  screen[:ascii[:OFFSET]] wypisz nametable 0 (32x30) jako indeksy kafelków w hex
                       (np. 24 24 0C 19...); `:ascii` renderuje drukowalne
                       wartości `indeks kafelka + OFFSET` jako znaki (tylko .nes)
                       Offset można podać dziesiętnie, jako `$hex` lub `0xhex`:
                       blargg = `$00`; AccuracyCoin = `$37` dla liter i `$30`
                       dla cyfr. AccuracyCoin ma osobne indeksy spacji i znaków
                       interpunkcyjnych, więc jeden offset nie odwzorowuje ich
                       pełnego fontu.
  pixels:X:Y:W:H       zrzuć wycinek framebuffera WxH od (X,Y) jako RGB w hex,
                       jedna linia na rząd (np. FF0000 00FF00 0000FF; tylko .nes)
  mem:ADDR:LEN         dump LEN bajtów z magistrali CPU od ADDR (hex/dec/$hex/0xhex)
  info                 wypisz aktualny frame count i cycle count
  pad1=BTNS            ustaw stan pada 1 (BTNS = lista po przecinku, puste = wyzeruj)
  pad1+BTN[,BTN..]     naciśnij wskazane przyciski (set bit)
  pad1-BTN[,BTN..]     puść wskazane przyciski (clear bit)
  pad2=... +... -...   to samo dla pada 2
  trace:CHAN:STATE     włącz/wyłącz kanał trace (CHAN=cpu|ppu|dma|apu, STATE=on|off)
  trace-file:PATH      zmień plik wyjściowy trace (domyślnie: trace.log)
  ac:PAGE:ROW          nawiguj menu AccuracyCoin do strony PAGE (1-based),
                      wiersza ROW (0-based), przez pad RIGHT/DOWN (20-frame timing)
  song:N               (tylko .nsf) initSong(N)
  next / prev          (tylko .nsf) przełącz na następny/poprzedni utwór
  songinfo             (tylko .nsf) wypisz nazwę/artystę/copyright/load/init/play

Przyciski: A, B, SELECT, START, UP, DOWN, LEFT, RIGHT

Trace zapisuje do `trace.log` jedną linię na clock. Każdy komponent (CPU,
PPU, DMA, APU) sam generuje swój wiersz w trakcie clockowania — TestRunner tylko
dodaje prefix `F=… CYC=…` i otwiera plik.

  - `ppu` (3 linie na cykl CPU): `F=… CYC=… PPU[SL=…,CY=…] PHASE V=… T=… fX=…
    W=… CTRL=… MASK=… STAT=… OAMA=… SPR=… NMI=… [ODD]
    BUS=… BD=… BR=… BUF=… BGP=…:… BGA=…:… OBUF=…`
      * PHASE = `PRE` / `IDLE` / `BG-FETCH` / `SPR-FETCH` / `BG-PREFTCH` /
        `NT-DUMMY` / `POST` / `VBLANK`
      * V/T = loopy `vramAddr`/`tramAddr`, fX = fine X, W = write latch
      * SPR = sprite count po evaluacji, ODD = marker klatki nieparzystej
      * BUS = fizyczna magistrala adresowa PPU (`ppuBusAddr`), BD = dane
        na magistrali (`ppuBusData`), BR = czy w tym cyklu był odczyt PPU
      * BUF = bufor odczytu PPUDATA (`ppuDataBuffer`)
      * BGP = rejestry przesuwne BG `lo:hi` (`bgPattern`), BGA = atrybuty
        BG `lo:hi` (`bgAttrib`) — przydatne przy testach BG Serial In
      * OBUF = bufor OAM (`oamDataBuffer`) — przydatne przy testach $2004
  - `cpu` (2 linie na cykl CPU):
      * `F=… CYC=… PHI1 PC=… A=… X=… Y=… S=… P=<flagi> R/W $XXXX[=DD][(nazwa)]
        MNEMONIC step ; symbol_PC` — stan rejestrów + decyzja na ten cykl
        (adres, kierunek, nazwa micro-stepu). Pojawia się gdy faza 1 się wykona.
      * `F=… CYC=… PHI2 fetched=XX` / `PHI2 wrote=XX` — dane faktycznie
        pobrane/zapisane na magistrali w fazie 2.
      * P: wielkie litery = bit ustawiony, małe = wyczyszczony
      * Anotacja `(nazwa)` dla rejestrów I/O i symboli z `.fns`/`.lbl`
  - `apu` (dopisywane na końcu linii CPU PHI1, zaraz po dekodowaniu instrukcji):
    `APU:DMC bytes=N buf=E/F halt=P/G addr=XXXX shift=XX bits=N tmr=N sil=S/s irq=I/i`
      * generowane przez `APU::emitTrace()`, wywoływane z `A2A03::clockPhi1()`
        zaraz po `dma.clockPhi1()` — pokazuje stan kanału DMC PO decyzjach DMA
        podjętych w tym cyklu
      * bytes = `dmc.bytesRemaining`, buf = czy sample buffer jest pusty
        (E=empty/F=full), halt = czy najbliższy DMA-halt wypadnie na cyklu put
        (P) czy get (G) — czyli `dmaHaltOnPut`
      * addr = `currentAddr` (adres następnego odczytu próbki), shift/bits =
        rejestr przesuwny wyjścia i liczba pozostałych bitów do przesunięcia
      * tmr = licznik okresu wyjścia (`timerCounter`), sil = silenceFlag
        (S=cisza, s=gra), irq = `irqPending` (I=ustawiony, i=brak)
  - `dma` (dopisywane na końcu linii PPU sub2, czyli PPU-klocku bezpośrednio
    po CPU PHI1, gdzie DMA podejmuje decyzję w `clockPhi2`):
    `DMA:<OAMphase>/<DMCphase> <action> @<addr> hb=0/1[->target(delay)]`
      * generowane przez sam DMA w `clockPhi2`, doklejane do ostatniej linii
      * hb = `hadBytesRecently.get()` — opóźniony (DelayedPin, 4 cykle) widok
        „próbka miała jeszcze bajty do odtworzenia w tej klatce” używany do
        wykrywania abortu DMA (explicit i implicit stop to ten sam mechanizm,
        patrz `nes_specs/dma.txt` sekcja Bugs); `->target(delay)` pojawia się
        tylko gdy pin ma zaplanowaną, jeszcze nie zakończoną zmianę

Łącznie z włączonym ppu+cpu+dma+apu: 5 linii na cykl CPU (3 PPU + PHI1 + PHI2),
wszystkie z tym samym `CYC=N`. Kolejność odzwierciedla faktyczne taktowanie:
PPU sub0, PPU sub1, CPU PHI1 (z APU suffix), PPU sub2 (z DMA suffix), CPU PHI2.

Przykłady:
  nes_test mytest.asm frames:60 screen mem:0x00:16
  nes_test mytest.asm trace:cpu:on trace:dma:on trace:apu:on frames:5
  nes_test ROM.nes frames:120 pad1+START frames:1 pad1-START frames:600 screen:ascii
  nes_test AccuracyCoin.asm frames:60 screen:ascii:0x37
  nes_test ROM.nes frames:120 pixels:0:0:16:8
  nes_test ROM.nes frames:600 reset frames:600 mem:0x6000:32
  nes_test song.nsf songinfo cycles:5000000 trace:cpu:on cycles:100000 info
  nes_test AccuracyCoin.asm ac:18:2 trace:cpu:on trace:ppu:on pad1+A frames:20 pad1-A frames:60

### Diagnozowanie zawieszeń / zdarzeń odległych w czasie

`cycles:N` nie zależy od żadnego stanu poza licznikiem cykli, więc jest
bezpieczne i tanie do przewijania naprzód bez trace (w przeciwieństwie do
`frames:N`, które może się zapętlić, jeśli licznik klatek rdzenia nigdy się
nie zmieni). Żeby złapać w trace zdarzenie, które występuje dopiero po
dłuższym czasie (np. utknięcie po kilku minutach grania):
1. Przewiń bez trace: `cycles:<duże_N>` (szybkie, bez narzutu I/O)
2. `info` — sprawdź frame/cycle count, bisekcją znajdź okolicę zdarzenia
3. Włącz `trace:cpu:on` (`trace:dma:on`/`trace:apu:on` w razie potrzeby) dopiero
   blisko interesującego miejsca, potem tylko kilkaset tysięcy cykli — inaczej
   plik trace.log spuchnie do dziesiątek MB/GB
4. Analizuj `trace.log`/plik z `trace-file:PATH` pod kątem adresu PC, na
   którym coś poszło nie tak

### Uruchamianie pojedynczego testu AccuracyCoin z trace

`accuracy_coin.exe` automatycznie naciska START i odpala wszystkie testy
naraz — nie da się wtedy włączyć trace tylko dla jednego (bo `trace.log` by
spuchł do gigabajtów). Zamiast tego buduj `AccuracyCoin.asm` przez `nes_test`,
ręcznie naprowadź menu pad-em na konkretny test, włącz trace tuż przed
naciśnięciem A i tylko ten test ląduje w log-u.

Menu AccuracyCoin:
- Strony testów (suity) wybiera się **RIGHT/LEFT** (kolumna)
- Pojedynczy test wybiera się **DOWN/UP** (wiersz)
- **A** uruchamia podświetlony test
- Suity są zdefiniowane w `AccuracyCoin.asm` jako etykiety `Suite_*`
  (kolejność = numer strony, kolejność `table ...` w suicie = numer wiersza)

Między każdym wciśnięciem a puszczeniem przycisku musi upłynąć ok. **20 klatek**,
żeby gra zdążyła przejść między ekranami (jedno wciśnięcie = pad1+X, frames:20,
pad1-X, frames:20).

Symbole `.fns` (generowane automatycznie przy buildzie `.asm`) ujawniają nazwę
funkcji testu w trace — np. `TEST_DeltaModulationChannel`,
`TEST_ExplicitDMAAbort`, `TEST_APULengthCounter`. Dzięki temu od razu wiadomo
który fragment AccuracyCoin.asm robi co — można porównać krok po kroku
sekwencję dostępów do magistrali z kodem źródłowym testu.

Przykład (DMC — strona 14, wiersz 6):
```
nes_test AccuracyCoin.asm frames:60 \
   pad1+RIGHT frames:20 pad1-RIGHT frames:20  ; powtórzyć 13× = strona 14 (zaczynamy od strony 1)
   ... \
   pad1+DOWN  frames:20 pad1-DOWN  frames:20  ; powtórzyć 6× = wiersz 6
   ... \
   trace:cpu:on trace:dma:on trace:apu:on \
   pad1+A frames:20 pad1-A frames:60
```

Co da analiza takiego trace:
- pełna sekwencja zapisów do APU/PPU robiona przez konkretny test
- odpowiada na pytania "jaki bajt poszedł do $4012", "kiedy DMC wystartował
  DMA", "jaki PC był aktywny w momencie failu" itd.
- kanał `apu` pokazuje wewnętrzny stan kanału DMC (bytesRemaining, sample
  buffer, shift register, timer, silence/IRQ flag) cykl po cyklu — przydatne
  przy błędach zależnych od dokładnego momentu wyczerpania próbki (np. DMA
  Abort, explicit/implicit stop) bez doklejania własnych printf-ów do APU.h
- dzięki anotacjom `(DMC_START)`, `(PPUCTRL)`, `(symbol_z_fns)` nie trzeba
  ręcznie korelować adresów z dokumentacją
- ułatwia napisanie minimalnego reproducera (`test/nes_template.asm`) gdy okaże
  się, że bug jest w jakimś konkretnym podetapie testu

### Szablon ASM (test/nes_template.asm)
Minimalny ROM gotowy do wypełnienia kodem testowym.
Krytyczne zasady formatu NESASM3:
- Wszystkie dyrektywy (.inesprg, .bank, .org, .word) MUSZĄ mieć wcięcie TAB
- .inesprg 2 = 32KB = 4 banki po 8KB (bank 0=$8000, 1=$A000, 2=$C000, 3=$E000)
- Kod testowy: .bank 2 / .org $C000
- Wektory: .bank 3 / .org $FFFA / .word nmi / .word reset / .word irq
- Przed VBlank looopem: wyłącz APU IRQ: lda #$40 / sta $4017
