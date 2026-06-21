# NES Emulator

Emulator NES robiony pod kątem zgodności z fizycznym sprzętem. Cel: zaliczenie wszystkich testów AccuracyCoin.

## Zasady

- Zachowanie kodu powinno odzwierciedlać zachowanie sprzętu — obsługa glitchy i edge-case'ów powinna wynikać naturalnie z implementacji, nie ze specjalnych przypadków.
- Unikaj dodawania zmiennych do klas, chyba że to konieczne — im więcej stanu, tym większe ryzyko rozjazdów.
- Dbaj o spójność i czytelność kodu. Jeśli nazewnictwo wprowadza zamieszanie, popraw je.
- Nie dodawaj komentarzy — dobry kod sam się komentuje.
- Do budowania używaj ninja.

## Ważne katalogi

- `nes_specs/` - cała specyfikacja NES, źródło prawdy. Może być niekompletna - jeśli czegoś brakuje, poinformuj, żeby uzupełnić.
- `nes-test-roms-master/` - ROM-y testowe; przy większości jest kod źródłowy z komentarzami oraz readme z oczekiwanymi wynikami.
- `test/` - kod narzędzi diagnostycznych (patrz niżej).

## Zalecany workflow diagnostyczny

Gdy test accuracy_coin nie przechodzi:
1. Przeczytaj opis błędu i kod źródłowy testu w `nes-test-roms-master/AccuracyCoin-main/AccuracyCoin.asm`
2. Napisz minimalny plik .asm odtwarzający konkretne scenario (szablon: `test/nes_template.asm`)
3. Uruchom `asm_run` z odpowiednim triggerem, żeby dostać trace.tsv
4. Przeanalizuj trace - każda linia to jeden cykl CPU, widać co robi DMA, PPU, CPU
5. Porównaj z oczekiwanym zachowaniem ze specyfikacji w `nes_specs/`
6. Możesz też odpalić inne testy, aby pozyskać więcej informacji

Dokumentacja różnych komponentów jest rozbita na wiele plików, więc pamiętaj, żeby sprawdzić wszystkie, bo każdy zawiera cenne informacje. Oszczędzi to wiele czasu przy analizie.
Aktualny stan testów: zbuduj `accuracy_coin` przez ninja, a następnie uruchom `cmake-build-release/accuracy_coin.exe`.

## Narzędzia diagnostyczne

### asm_run (cmake-build-release/asm_run.exe)
Kompiluje plik .asm przez nesasm3 i uruchamia w headless NES.
Użyteczne do pisania minimalnych testów izolujących konkretne zachowanie hardware.

Użycie:
  asm_run [opcje] <plik.asm>

Opcje:
  --frames N          liczba klatek do emulacji (domyślnie: 300)
  --trigger SPEC      włącz cycle tracer z triggerem (patrz niżej)
  --pre N             cykli przed triggerem do zapisania (domyślnie: 300)
  --post N            cykli po triggerze do zapisania (domyślnie: 300)
  --trace-file FILE   plik wyjściowy tracera (domyślnie: trace.tsv)
  --nesasm PATH       ścieżka do nesasm.exe (domyślnie: auto-detect)

Triggery (--trigger):
  oam          - zapis do $4014 (start OAM DMA)
  dmc          - start DMC DMA (zmiana fazy z Idle)
  nmi          - zbocze NMI (linia idzie low)
  pc:XXXX      - CPU PC osiąga adres hex XXXX
  addr:XXXX    - dowolny zapis do adresu hex XXXX

Wyjście:
  stdout - zawartość ekranu (nametable, tak jak nes_test)
  plik trace.tsv - cycle-accurate log TSV z kolumnami:
    CYC, PC, A, X, Y, SP, P, BUS_ADDR, BUS_DATA, RW, DMA_ACT,
    DMC_PH, OAM_PH, ACTION, DMA_ADDR, PPU_SL, PPU_CY, NMI, IRQ

Przykład:
  asm_run --trigger oam --pre 10 --post 520 test\oam_dma_test.asm
  # Potem grep: Select-String "OAMGet|OAMPut" trace.tsv

### nes_test (cmake-build-release/nes_test.exe)
Odpala wskazany ROM na kilka sekund i zwraca zawartość ekranu.
Użyteczne np. do testów blargga. Po podaniu katalogu odpala wszystkie testy w nim (recursive).

### accuracy_coin (cmake-build-release/accuracy_coin.exe)
Odpala AccuracyCoin.nes i zwraca listę testów z wynikami pass/fail.

### Szablon ASM (test/nes_template.asm)
Minimalny ROM gotowy do wypełnienia kodem testowym.
Krytyczne zasady formatu NESASM3:
- Wszystkie dyrektywy (.inesprg, .bank, .org, .word) MUSZĄ mieć wcięcie TAB
- .inesprg 2 = 32KB = 4 banki po 8KB (bank 0=$8000, 1=$A000, 2=$C000, 3=$E000)
- Kod testowy: .bank 2 / .org $C000
- Wektory: .bank 3 / .org $FFFA / .word nmi / .word reset / .word irq
- Przed VBlank looopem: wyłącz APU IRQ: lda #$40 / sta $4017

### CycleTracer (test/CycleTracer.h) + TracedNESHeadlessSystem (test/TracedNESHeadlessSystem.h)
Klasy do użycia w niestandardowych harness-ach testowych (C++).
TracedNESHeadlessSystem dziedziczy po NESHeadlessSystem - wystarczy wywołać attachTracer(config) przed uruchomieniem.
