#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

int should_generate_passenger() {
    return (rand() % 100) < 5; // 5% szans na pojawienie się pasażera
}

void generate_passenger() {
    int age = rand() % 56 + 15; // Wiek: 15–70
    // Możesz tutaj dodać inne logiki związane z pasażerem
    // W tym uproszczonym przykładzie po prostu generujemy wiek
}

int main() {
    srand(time(NULL)); // Inicjalizacja generatora liczb losowych

    while (1) {
        if (should_generate_passenger()) {
            generate_passenger();
        }
        sleep(rand() % 3 + 2); // Losowy czas oczekiwania 2–4 sekundy
    }

    return 0;
}
