#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <signal.h>
#include <string.h>
#include <errno.h>

#define SHM_KEY 5678
#define MSG_KEY 1234
#define BOAT1_CAPACITY 10
#define BOAT2_CAPACITY 8

struct msgbuf {
    long mtype;
    char mtext[100];
};

float revenue = 0.0;
int boat1_available_seats = BOAT1_CAPACITY;
int boat2_available_seats = BOAT2_CAPACITY;

int msqid = -1;
int shmid = -1;
char *czas = NULL;

void cleanup(int signum) {
    printf("\nZwalnianie zasobów kasjera...\n");

    if (czas != NULL) {
        if (shmdt(czas) == -1) {
            perror("Nie można odłączyć pamięci współdzielonej");
        }
    }

    if (msqid != -1) {
        if (msgctl(msqid, IPC_RMID, NULL) == -1) {
            perror("Nie można usunąć kolejki komunikatów");
        }
    }

    exit(0);
}

void get_simulated_time(char *buffer) {
    shmid = shmget(SHM_KEY, sizeof(char) * 20, 0666);
    if (shmid == -1) {
        perror("Nie można połączyć się z pamięcią współdzieloną");
        cleanup(0);
    }

    czas = shmat(shmid, NULL, 0);
    if (czas == (char *)-1) {
        perror("Nie można przyłączyć pamięci współdzielonej");
        cleanup(0);
    }

    snprintf(buffer, 20, "%s", czas);

    if (shmdt(czas) == -1) {
        perror("Nie można odłączyć pamięci współdzielonej po odczycie");
    }
    czas = NULL;
}

int is_within_operating_hours(const char *time) {
    int hour, minute, second;
    sscanf(time, "%d:%d:%d", &hour, &minute, &second);
    return (hour >= 6 && hour < 23); // Praca kasjera między 6:00 a 23:00
}

float calculate_ticket_cost(int age) {
    if (age < 3) {
        return 0.0;
    } else {
        return 20.0;
    }
}

const char* assign_boat(int age, int *seats_needed) {
    if (age < 15) {
        *seats_needed = 2; // Dziecko z opiekunem
        if (boat2_available_seats >= *seats_needed) {
            boat2_available_seats -= *seats_needed;
            return "łódź nr 2 (z opiekunem)";
        } else {
            return "Kolejka na łódź nr 2 (brak miejsc)";
        }
    } else if (age > 70) {
        if (boat2_available_seats >= 1) {
            boat2_available_seats--;
            return "łódź nr 2";
        } else {
            return "Kolejka na łódź nr 2 (brak miejsc)";
        }
    } else {
        if (boat1_available_seats >= 1) {
            boat1_available_seats--;
            return "łódź nr 1";
        } else if (boat2_available_seats >= 1) {
            boat2_available_seats--;
            return "łódź nr 2";
        } else {
            return "Kolejka na obie łodzie (brak miejsc)";
        }
    }
}

int should_generate_passenger() {
    // Zmniejszona szansa na pojawienie się pasażera (np. 20% szans)
    return (rand() % 100) < 20;
}

int main() {
    signal(SIGINT, cleanup);
    signal(SIGTERM, cleanup);

    msqid = msgget(MSG_KEY, IPC_CREAT | 0666);
    if (msqid == -1) {
        perror("Nie można utworzyć kolejki komunikatów");
        exit(1);
    }

    struct msgbuf msg;
    char simulated_time[20];

    while (1) {
        get_simulated_time(simulated_time);

        if (!is_within_operating_hours(simulated_time)) {
            printf("[%s] Kasjer: Zamknięte, otwarte od 6:00.\n", simulated_time);
            sleep(1);
            continue;
        }

        if (!should_generate_passenger()) {
            sleep(1); // Oczekiwanie na losowe pojawienie się pasażera
            continue;
        }

        if (msgrcv(msqid, &msg, sizeof(msg.mtext), 1, IPC_NOWAIT) == -1) {
            if (errno != ENOMSG) {
                perror("Błąd podczas odbioru wiadomości");
            }
            sleep(1); // Brak wiadomości, chwilowe oczekiwanie
            continue;
        }

        int age, passenger_id;
        sscanf(msg.mtext, "Pasażer (wiek: %d, PID: %d)", &age, &passenger_id);

        // Blokowanie pasażerów poniżej 15 lat bez opiekuna
        if (age < 15) {
            printf("[%s] Kasjer: Odrzucono pasażera (PID: %d, wiek: %d) - brak opiekuna.\n", 
                   simulated_time, passenger_id, age);
            continue;
        }

        int seats_needed = 1; // Domyślnie 1 miejsce
        float cost = calculate_ticket_cost(age);
        const char* boat_assignment = assign_boat(age, &seats_needed);

        if (strstr(boat_assignment, "Kolejka") != NULL) {
            printf("[%s] Kasjer: Pasażer (PID: %d, wiek: %d) czeka na kolejny rejs: %s.\n",
                   simulated_time, passenger_id, age, boat_assignment);
            continue; // Pasażer czeka na kolejną okazję
        }

        if (cost == 0.0) {
            printf("[%s] Kasjer: Pasażer (PID: %d, wiek: %d) płynie za darmo. Przydział: %s.\n",
                   simulated_time, passenger_id, age, boat_assignment);
        } else {
            revenue += cost;
            printf("[%s] Kasjer: Pasażer (PID: %d, wiek: %d) zapłacił %.2f PLN. Przydział: %s.\n",
                   simulated_time, passenger_id, age, cost, boat_assignment);
        }
    }

    cleanup(0);
    return 0;
}
