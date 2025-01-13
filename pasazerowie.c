#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

int should_generate_passenger_with_dependant() {
    return (rand() % 100) < 15; // 15% szans na pojawienie się pasażera z osobą zależną
}

void generate_passenger_with_dependant() {
    int parent_age = rand() % 56 + 15; // Wiek rodzica: 15–70
    pid_t parent_pid = getpid();

    pid_t dependant_pid = fork();
    if (dependant_pid == 0) { // Proces potomny
        int dependant_age;
        if (rand() % 2 == 0) {
            dependant_age = rand() % 14 + 1; // Wiek dziecka: 1–14
        } else {
            dependant_age = rand() % 30 + 71; // Wiek seniora: 71–100
        }
        // Proces potomny kończy się natychmiast.
        exit(0);
    } else if (dependant_pid > 0) {
        // Proces rodzica kontynuuje działanie.
    } else {
        perror("Nie można utworzyć procesu potomnego");
    }
}

int main() {
    srand(time(NULL));

    while (1) {
        if (should_generate_passenger_with_dependant()) {
            generate_passenger_with_dependant();
        }
        sleep(3); // Czekaj 3 sekundy przed kolejną próbą generowania
    }

    return 0;
}

