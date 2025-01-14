#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

void generate_passenger_with_dependant() {
    int parent_age = rand() % 56 + 15; // Wiek rodzica: 15–70
    int dependant_age;

    if (rand() % 2 == 0) {
        dependant_age = rand() % 14 + 1; // Wiek dziecka: 1–14
    } else {
        dependant_age = rand() % 30 + 71; // Wiek seniora: 71–100
    }

    pid_t parent_pid = getpid();
    pid_t dependant_pid = fork();

    if (dependant_pid == 0) { // Proces potomny
        printf("Pasażer z osobą zależną: PID rodzica: %d, wiek rodzica: %d, PID osoby zależnej: %d, wiek: %d\n",
               parent_pid, parent_age, getpid(), dependant_age);
        exit(0);
    } else if (dependant_pid > 0) { // Proces rodzica
        printf("Utworzono proces potomny: PID rodzica: %d, PID potomka: %d, wiek rodzica: %d, wiek osoby zależnej: %d\n",
               parent_pid, dependant_pid, parent_age, dependant_age);
    } else {
        perror("Nie można utworzyć procesu potomnego");
    }
}

int main() {
    srand(time(NULL));

    while (1) {
        generate_passenger_with_dependant();
        sleep(2); // Symulacja przerw między generowaniem pasażerów
    }

    return 0;
}
