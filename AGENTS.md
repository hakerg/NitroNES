Jest to emulator NES, robiony pod kątem zgodności z fizycznym sprzętem i dokładności.
Ostatecznym celem repozytorium jest zaliczenie wszystkich testów accuracy coin.
Zachowanie kodu powinno odzwierciedlać zachowanie sprzętu, żeby obsługa wszelkich glitchy/edge-case-ów wychodziła naturalnie.
Starajmy się unikać dodawania zmiennych do klas, chyba że to konieczne - im więcej zmiennych, tym większe ryzyko rozjazdów.
Możesz używać ninja do budowania.

Ważne katalogi:
 - nes_specs - cała specyfikacja NES, źródło prawdy, emulator musi być z nią zgodny. Jeśli czegoś w niej brakuje, poinformuj, żeby uzupełnić
 - nes-test-roms-master - są tam ROM-y testowe, przy każdym istotnym jest kod źródłowy z komentarzami oraz readme z oczekiwanymi wynikami
 - test - zawiera kod do executable accuracy_coin i nes_test:
   - accuracy_coin_main.cpp - odpala wszystkie testy accuracy coin i zwraca ładnie sformatowane info, co jest jeszcze nie tak z emulatorem. Kod źródłowy accuracy coin (nes-test-roms-master\AccuracyCoin-main) zawiera wiele użytecznych komentarzy.
   - nes_test_main.cpp - odpala wskazany ROM na kilka sekund i zwraca zawartość ekranu - użyteczne np. do testów blargga. Po podaniu katalogu odpala wszystkie testy w nim (recursive)