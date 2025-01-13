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
    char mtext[200];
};

float revenue = 0.0;
int boat1_available_seats = BOAT1_CAPACITY;
int boat2_available_seats = BOAT2_CAPACITY;

int msqid = -1;
int shmid = -1;
char *czas = NULL;

void cleanup(int signum) {
    (void)signum; // Ignorowanie parametru
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
    if (sscanf(time, "%d:%d:%d", &hour, &minute, &second) != 3) {
        fprintf(stderr, "Niepoprawny format czasu: %s\n", time);
        return 0;
    }
    return (hour >= 6 && hour < 23); // Praca między 6:00 a 23:00
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

        if (msgrcv(msqid, &msg, sizeof(msg.mtext), 0, IPC_NOWAIT) == -1) {
            if (errno != ENOMSG) {
                perror("Błąd podczas odbioru wiadomości");
            }
            sleep(1);
            continue;
        }

        if (msg.mtype == 1) {
            int age, pid;
            if (sscanf(msg.mtext, "Pasażer (wiek: %d, PID: %d)", &age, &pid) == 2) {
                handle_passenger(simulated_time, pid, age);
            }
        } else if (msg.mtype == 2) {
            int parent_pid, parent_age, dependant_pid, dependant_age;
            if (sscanf(msg.mtext, "Pojawił się pasażer z dzieckiem/seniorem PID rodzica: %d, wiek: %d, PID dziecka/seniora: %d, wiek: %d",
                       &parent_pid, &parent_age, &dependant_pid, &dependant_age) == 4) {
                handle_passenger_with_dependant(simulated_time, parent_pid, parent_age, dependant_pid, dependant_age);
            }
        }
    }

    return 0;
}
