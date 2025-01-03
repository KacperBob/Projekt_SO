#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <signal.h>
#include <string.h>

#define SHM_KEY 5678
#define MSG_KEY 1234
#define BOAT1_CAPACITY 10
#define BOAT2_CAPACITY 8

struct msgbuf {
    long mtype;
    char mtext[100];
};

// Zmienna globalna do przechowywania dochodów
float revenue = 0.0;

// Zmienna globalna do śledzenia dostępnych miejsc na łodziach
int boat1_available_seats = BOAT1_CAPACITY;
int boat2_available_seats = BOAT2_CAPACITY;

int msqid = -1; // Kolejka komunikatów
int shmid = -1; // Segment pamięci współdzielonej
char *czas = NULL; // Wskaźnik do pamięci współdzielonej

// Funkcja czyszcząca zasoby
void cleanup(int signum) {
    printf("\nZwalnianie zasobów...\n");

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

// Funkcja do pobierania czasu symulowanego
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

// Funkcja obliczająca koszt biletu
float calculate_ticket_cost(int age, int is_repeating) {
    if (age < 3) {
        return 0.0; // Darmowy bilet
    } else if (is_repeating) {
        return 10.0; // Bilet z 50% zniżką
    } else {
        return 20.0; // Pełna cena
    }
}

// Funkcja przydzielająca pasażerów do łodzi
const char* assign_boat(int age, int group_with_adult, int *seats_needed, int is_repeating) {
    if (age < 15) {
        if (group_with_adult) {
            if (boat2_available_seats >= *seats_needed) {
                boat2_available_seats -= *seats_needed;
                return "Łódź nr 2 (z opiekunem)";
            } else {
                return "Odrzucony (brak miejsc na łodzi nr 2 dla grupy)";
            }
        } else {
            return "Odrzucony (wymagana opieka osoby dorosłej)";
        }
    } else if (age > 70) {
        if (boat2_available_seats >= 1) {
            boat2_available_seats--;
            return "Łódź nr 2";
        } else {
            return "Odrzucony (brak miejsc na łodzi nr 2)";
        }
    } else {
        if (is_repeating || boat1_available_seats >= 1) {
            if (boat1_available_seats >= 1) {
                boat1_available_seats--;
                return "Łódź nr 1";
            } else if (boat2_available_seats >= 1) {
                boat2_available_seats--;
                return "Łódź nr 2";
            }
        }
        return "Odrzucony (brak miejsc na obu łodziach)";
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

    struct msgbuf msg;
    char simulated_time[20];

    while (1) {
        if (msgrcv(msqid, &msg, sizeof(msg.mtext), 1, 0) == -1) {
            perror("Błąd podczas odbioru wiadomości");
            sleep(1);
            continue;
        }

        int age, passenger_id, is_repeating;
        int group_with_adult = 0;
        int seats_needed = 1;
        sscanf(msg.mtext, "Pasażer (wiek: %d, PID: %d, powtarzający: %d)", &age, &passenger_id, &is_repeating);

        if (age < 15) {
            group_with_adult = 1;
            seats_needed = 2;
        }

        float cost = calculate_ticket_cost(age, is_repeating);
        const char* boat_assignment = assign_boat(age, group_with_adult, &seats_needed, is_repeating);

        get_simulated_time(simulated_time);

        if (cost == 0.0) {
            printf("[%s] Kasjer: Pasażer (PID: %d, wiek: %d) otrzymał darmowy bilet. Przydział: %s\n", simulated_time, passenger_id, age, boat_assignment);
        } else {
            printf("[%s] Kasjer: Pasażer (PID: %d, wiek: %d) zapłacił %.2f zł za bilet. Przydział: %s\n", simulated_time, passenger_id, age, cost, boat_assignment);
            revenue += cost;
        }

        printf("Aktualne dochody: %.2f zł\n", revenue);
        printf("Dostępne miejsca: Łódź 1: %d, Łódź 2: %d\n", boat1_available_seats, boat2_available_seats);
    }

    cleanup(0);
    return 0;
}
