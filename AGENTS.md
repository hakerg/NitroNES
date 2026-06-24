# NES Emulator

Emulator NES robiony pod kątem zgodności z fizycznym sprzętem. Cel: zaliczenie wszystkich testów AccuracyCoin.

## Zasady

- Zachowanie kodu powinno odzwierciedlać zachowanie sprzętu — obsługa glitchy i edge-case'ów powinna wynikać naturalnie z implementacji, nie ze specjalnych przypadków.
- Unikaj dodawania zmiennych do klas, chyba że to konieczne — im więcej stanu, tym większe ryzyko rozjazdów.
- Dbaj o spójność i czytelność kodu. Jeśli nazewnictwo wprowadza zamieszanie, popraw je.
- Nie dodawaj komentarzy — dobry kod sam się komentuje.
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
3. Uruchom `nes_test` z odpowiednimi komendami, włącz trace cpu/ppu/dma — zapis trafi do `trace.log`
4. Przeanalizuj trace - każda linia to jeden cykl CPU, widać co robi DMA, PPU, CPU; symbole z `.fns` pojawiają się automatycznie
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
- `*.asm` — kompiluje przez nesasm3 (in-place, obok źródła pojawia się
  `*.nes` + `*.fns` z symbolami)
- `*.s`  — kompiluje przez ca65 + linkuje przez ld65 (z `--feature force_range`
  dla zgodności z kodem blargga); artefakty trafiają do
  `%TEMP%/nes_test_build/<hash>/<basename>/` — repo nigdy nie jest dotykane.
  `nes.cfg` i katalog `common/` są wyszukiwane automatycznie obok pliku
  źródłowego (w górę drzewa do 4 poziomów).

Symbole z `.fns` (nesasm) lub `.lbl` w formacie VICE (ld65) są ładowane
automatycznie z miejsca obok wynikowego `.nes` i pojawiają się w trace CPU.

Użycie:
  nes_test <rom-or-asm> [command]...

Komendy (wykonywane sekwencyjnie, każda jako osobny argv):
  frames:N             clockuj N klatek PPU
  cycles:N             clockuj N cykli CPU
  reset                hard reset
  screen               wypisz nametable 0 (32x30 znaków) na stdout
  mem:ADDR:LEN         dump LEN bajtów z magistrali CPU od ADDR (hex/dec/$hex/0xhex)
  pad1=BTNS            ustaw stan pada 1 (BTNS = lista po przecinku, puste = wyzeruj)
  pad1+BTN[,BTN..]     naciśnij wskazane przyciski (set bit)
  pad1-BTN[,BTN..]     puść wskazane przyciski (clear bit)
  pad2=... +... -...   to samo dla pada 2
  trace:CHAN:STATE     włącz/wyłącz kanał trace (CHAN=cpu|ppu|dma, STATE=on|off)
  trace-file:PATH      zmień plik wyjściowy trace (domyślnie: trace.log)

Przyciski: A, B, SELECT, START, UP, DOWN, LEFT, RIGHT

Trace zapisuje do `trace.log` jedną linię na clock — albo CPU (po phi2), albo
PPU (po każdym z 3 sub-tików). Linie PPU i CPU nigdy nie są mieszane.

  - `ppu` (3 linie na cykl CPU): `F=… CYC=… PPU[SL=…,CY=…] PHASE V=… T=… fX=…
    W=… CTRL=… MASK=… STAT=… OAMA=… SPR=… NMI=… [ODD]`
      * PHASE = `PRE` / `IDLE` / `BG-FETCH` / `SPR-FETCH` / `BG-PREFTCH` /
        `NT-DUMMY` / `POST` / `VBLANK`
      * V/T = loopy `vram_addr`/`tram_addr`, fX = fine X, W = write latch
      * SPR = sprite count po evaluacji, ODD = marker klatki nieparzystej
  - `cpu` (1 linia na cykl CPU): `F=… CYC=… PC=… A=… X=… Y=… S=… P=<flagi>
    R/W $XXXX=DD[(nazwa)]  MNEMONIC step  ; symbol_PC`
      * P: wielkie litery = bit ustawiony, małe = wyczyszczony
      * Anotacja `$XXXX=DD(nazwa)` dla rejestrów I/O i symboli z `.fns`/`.lbl`
      * MNEMONIC i nazwa micro-stepu (np. `STA am_abs_2`, `jsr3_pushPCH`)
      * R/W odzwierciedla rzeczywistą akcję magistrali — gdy DMA przejmuje,
        pokazywany jest jego kierunek
  - `dma` (dopisywane na końcu linii CPU): `DMA:<OAMphase>/<DMCphase> <action>
    @<addr>`

Łącznie z włączonym ppu+cpu+dma: 4 linie na cykl CPU (3 PPU + 1 CPU+DMA),
wszystkie z tym samym `CYC=N`. PPU CY rośnie monotonicznie o 1 między liniami.

Przykłady:
  nes_test mytest.asm frames:60 screen mem:0x00:16
  nes_test mytest.asm trace:cpu:on trace:dma:on frames:5
  nes_test ROM.nes frames:120 pad1+START frames:1 pad1-START frames:600 screen
  nes_test ROM.nes frames:600 reset frames:600 mem:0x6000:32

### Szablon ASM (test/nes_template.asm)
Minimalny ROM gotowy do wypełnienia kodem testowym.
Krytyczne zasady formatu NESASM3:
- Wszystkie dyrektywy (.inesprg, .bank, .org, .word) MUSZĄ mieć wcięcie TAB
- .inesprg 2 = 32KB = 4 banki po 8KB (bank 0=$8000, 1=$A000, 2=$C000, 3=$E000)
- Kod testowy: .bank 2 / .org $C000
- Wektory: .bank 3 / .org $FFFA / .word nmi / .word reset / .word irq
- Przed VBlank looopem: wyłącz APU IRQ: lda #$40 / sta $4017

