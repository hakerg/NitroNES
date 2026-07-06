#pragma once
#include <array>

template <typename T>
class DelayedPin {
public:
    DelayedPin(T initial = T{}) : current(initial), target(initial), countdown(-1) {}

    // Planuje zmianę stanu na wartość `new_target` za `delay` cykli/taktów
    void set(T new_target, int delay) {
        target = new_target;
        countdown = delay;
        if (countdown <= 0) {
            current = target;
            countdown = -1;
        }
    }

    // Posuwa czas (timer) do przodu. Zwraca `true` DOKŁADNIE w momencie, gdy licznik
    // osiągnie 0 i nowa wartość zastąpi starą.
    bool tick() {
        if (countdown > 0) {
            countdown--;
            if (countdown == 0) {
                current = target;
                countdown = -1;
                return true;
            }
        } else if (countdown == 0) {
            current = target;
            countdown = -1;
            return true;
        } else {
            // Gdy nie oczekujemy na opóźnienie, śledzimy target by zachować spójność
            current = target;
        }
        return false;
    }

    // Natychmiastowo nadpisuje stan, resetując jakiekolwiek opóźnienia
    void force(T val) {
        current = val;
        target = val;
        countdown = -1;
    }

    T get() const { return current; }
    T getTarget() const { return target; }
    bool isPending() const { return countdown >= 0; }
    int getDelay() const { return countdown; }

    // Zwraca stan, w jakim pin znajdował się określoną liczbę cykli w tył względem tarczy docelowej
    // (symuluje rejestr przesuwny i sprawdzenie wczesnego odczepu potoku).
    T getTap(int cycles_ago) const {
        if (countdown < 0) return current;
        if (countdown <= cycles_ago) return target;
        return current;
    }

    operator T() const { return current; }
    DelayedPin& operator=(T val) { force(val); return *this; }

private:
    T current;
    T target;
    int countdown;
};

// Rejestr przesuwny o stałej głębokości N: w odróżnieniu od DelayedPin (który
// modeluje POJEDYNCZE zaplanowane przejście), opóźnia CIĄGŁY sygnał o dokładnie
// N taktów, poprawnie nawet gdy wartość zmienia się co takt (np. impuls trwający
// wiele kolejnych taktów). N jest parametrem szablonu, bo to stała sprzętowa
// znana w czasie kompilacji — brak alokacji, brak osobnego licznika do pomylenia.
template <typename T, int N>
class ShiftDelay {
public:
    explicit ShiftDelay(T initial = T{}) { buf.fill(initial); }

    // Wsuwa `value` na bieżący takt, zwraca wartość sprzed dokładnie N taktów.
    T tick(T value) {
        T out = buf[head];
        buf[head] = value;
        head = (head + 1) % N;
        return out;
    }

    void force(T val) { buf.fill(val); }

private:
    std::array<T, N> buf{};
    int head = 0;
};
