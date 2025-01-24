#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <string.h>

#define FIFO_POMOST "fifo_pomost"

// Funkcja losowa decydująca, czy generować pasażera (zmniejszona szansa, np. 20%)
int should_generate_passenger() {
    return (rand() % 100) < 20; // 20% szans na pojawienie się pasażera
}

// Generowanie pasażera z osobą zależną
void generate_passenger_with_dependant() {
    int parent_age = rand() % 56 + 15; // Wiek rodzica: 15–70
    int dependant_age;
    pid_t parent_pid = getpid();

    if (rand() % 2 == 0) {
        dependant_age = rand() % 14 + 1; // Wiek dziecka: 1–14
    } else {
        dependant_age = rand() % 30 + 71; // Wiek seniora: 71–100
    }

    pid_t dependant_pid = fork();
    char message[256];

    if (dependant_pid == 0) { // Proces potomny
        snprintf(message, sizeof(message), "PID: %d, Wiek osoby zależnej: %d\n", getpid(), dependant_age);
        int fifo_fd = open(FIFO_POMOST, O_WRONLY | O_NONBLOCK);
        if (fifo_fd != -1) {
            write(fifo_fd, message, strlen(message));
            close(fifo_fd);
        } else {
            perror("Nie można otworzyć FIFO_POMOST");
        }
        exit(0);
    } else if (dependant_pid > 0) { // Proces rodzica
        snprintf(message, sizeof(message), "PID: %d, Wiek rodzica: %d\n", parent_pid, parent_age);
        int fifo_fd = open(FIFO_POMOST, O_WRONLY | O_NONBLOCK);
        if (fifo_fd != -1) {
            write(fifo_fd, message, strlen(message));
            close(fifo_fd);
        } else {
            perror("Nie można otworzyć FIFO_POMOST");
        }
    } else {
        perror("Nie można utworzyć procesu potomnego");
    }
}

int main() {
    srand(time(NULL));

    while (1) {
        if (should_generate_passenger()) { // Losowa szansa na generowanie pasażerów
            generate_passenger_with_dependant();
        }
        sleep(2); // Symulacja przerw między generowaniem pasażerów
    }

    return 0;
}
