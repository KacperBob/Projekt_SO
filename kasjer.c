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
    printf("\nZwalnianie zasobów kasjera...\n");

    if (czas != NULL) {
        shmdt(czas);
    }

    if (msqid != -1) {
        msgctl(msqid, IPC_RMID, NULL);
    }

    exit(0);
}

void initialize_shared_memory() {
    shmid = shmget(SHM_KEY, sizeof(char) * 20, 0666);
    if (shmid == -1) {
        perror("Nie można połączyć się z pamięcią współdzieloną (spróbuj uruchomić nowyczas)");
        exit(1);
    }

    czas = shmat(shmid, NULL, 0);
    if (czas == (char *)-1) {
        perror("Nie można przyłączyć pamięci współdzielonej");
        exit(1);
    }
}

int is_within_operating_hours(const char *time) {
    int hour, minute, second;
    if (sscanf(time, "%d:%d:%d", &hour, &minute, &second) != 3) {
        fprintf(stderr, "Niepoprawny format czasu: %s\n", time);
        return 0;
    }
    return (hour >= 6 && hour < 23);
}

float calculate_ticket_cost(int age) {
    if (age < 3) {
        return 0.0;
    } else if (age >= 4 && age <= 14) {
        return 10.0;
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

int main() {
    signal(SIGINT, cleanup);
    signal(SIGTERM, cleanup);

    msqid = msgget(MSG_KEY, IPC_CREAT | 0666);
    if (msqid == -1) {
        perror("Nie można utworzyć kolejki komunikatów");
        exit(1);
    }

    initialize_shared_memory();

    struct msgbuf msg;

    while (1) {
        if (!czas || czas[0] == '\0') {
            fprintf(stderr, "Czas nie jest dostępny. Oczekiwanie na nowyczas...\n");
            sleep(1);
            initialize_shared_memory();
            continue;
        }

        if (!is_within_operating_hours(czas)) {
            printf("[%s] Kasjer: Zamknięte, otwarte od 6:00.\n", czas);
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

        int age, pid;
        if (sscanf(msg.mtext, "Pasażer (wiek: %d, PID: %d)", &age, &pid) == 2) {
            handle_passenger(czas, pid, age);
        } else {
            fprintf(stderr, "Niepoprawny format wiadomości: %s\n", msg.mtext);
        }
    }

    return 0;
}
