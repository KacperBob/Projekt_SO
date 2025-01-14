#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

#define BOAT1_CAPACITY 10
#define BOAT2_CAPACITY 8

float revenue = 0.0;
int boat1_available_seats = BOAT1_CAPACITY;
int boat2_available_seats = BOAT2_CAPACITY;

void cleanup(int signum) {
    (void)signum; // Ignorowanie parametru
    printf("\nZatrzymuję kasjera...\n");
    printf("Ostateczny przychód: %.2f PLN\n", revenue);
    exit(0);
}

float calculate_ticket_cost(int age) {
    if (age < 3) {
        return 0.0;
    } else if (age >= 4 && age <= 14) {
        return 10.0; // Zniżka 50%
    } else {
        return 20.0;
    }
}

void handle_passenger(const char *time, int pid, int age) {
    float cost = calculate_ticket_cost(age);
    const char *boat_assignment;

    if (boat1_available_seats > 0) {
        boat1_available_seats--;
        boat_assignment = "Łódź nr 1";
    } else if (boat2_available_seats > 0) {
        boat2_available_seats--;
        boat_assignment = "Łódź nr 2";
    } else {
        printf("[%s] Kasjer: Pasażer (PID: %d, wiek: %d) odrzucony - brak miejsc.\n", time, pid, age);
        return;
    }

    if (cost == 0.0) {
        printf("[%s] Kasjer: Pasażer (PID: %d, wiek: %d) płynie za darmo. Przydział: %s.\n", time, pid, age, boat_assignment);
    } else {
        revenue += cost;
        printf("[%s] Kasjer: Pasażer (PID: %d, wiek: %d) zapłacił %.2f PLN. Przydział: %s.\n", time, pid, age, cost, boat_assignment);
    }
}

void handle_passenger_with_dependant(const char *time, int parent_pid, int parent_age, int dependant_pid, int dependant_age) {
    float parent_cost = calculate_ticket_cost(parent_age);
    float dependant_cost = calculate_ticket_cost(dependant_age);
    float total_cost = parent_cost + dependant_cost;

    if (boat2_available_seats >= 2) {
        boat2_available_seats -= 2;
        printf("[%s] Kasjer: Pasażerowie (PID rodzica: %d, wiek: %d, PID dziecka/seniora: %d, wiek: %d) zapłacili %.2f PLN. Przydział: Łódź nr 2 dla obu.\n",
               time, parent_pid, parent_age, dependant_pid, dependant_age, total_cost);
        revenue += total_cost;
    } else {
        printf("[%s] Kasjer: Pasażerowie (PID rodzica: %d, PID dziecka/seniora: %d) odrzuceni - brak miejsc na łodzi nr 2.\n",
               time, parent_pid, dependant_pid);
    }
}

int main() {
    struct sigaction sa;
    sa.sa_handler = cleanup;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    srand(time(NULL));

    char simulated_time[20];
    while (1) {
        // Symulacja czasu
        time_t now = time(NULL);
        struct tm *local_time = localtime(&now);
        snprintf(simulated_time, sizeof(simulated_time), "%02d:%02d:%02d",
                 local_time->tm_hour, local_time->tm_min, local_time->tm_sec);

        int mode = rand() % 2; // Wybór między pasażerem pojedynczym a z osobą zależną
        if (mode == 0) {
            // Pojedynczy pasażer
            int pid = rand() % 10000 + 1; // Losowy PID
            int age = rand() % 56 + 15;  // Wiek: 15–70
            handle_passenger(simulated_time, pid, age);
        } else {
            // Pasażer z osobą zależną
            int parent_pid = rand() % 10000 + 1;    // Losowy PID rodzica
            int dependant_pid = rand() % 10000 + 1; // Losowy PID dziecka/seniora
            int parent_age = rand() % 56 + 15;      // Wiek rodzica: 15–70
            int dependant_age = (rand() % 2 == 0) ? (rand() % 14 + 1) : (rand() % 30 + 71); // Wiek dziecka: 1–14, seniora: 71–100
            handle_passenger_with_dependant(simulated_time, parent_pid, parent_age, dependant_pid, dependant_age);
        }

        sleep(1); // Symulacja przetwarzania
    }

    return 0;
}
