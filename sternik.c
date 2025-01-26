#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/sem.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <time.h>

#define SHM_KEY_POMOST1 7890
#define SHM_KEY_POMOST2 7891
#define FIFO_STERNIK1 "fifo_ster1"
#define FIFO_STERNIK2 "fifo_ster2"
#define MAX_PASSENGERS_BOAT1 50
#define MAX_PASSENGERS_BOAT2 40
#define TRIP_DURATION 5 // Czas trwania rejsu w sekundach

struct pomost_state {
    int passengers_on_bridge;
    int direction; // 1 for boarding, -1 for leaving, 0 for idle
};

int shmid_pomost1, shmid_pomost2;
struct pomost_state *pomost1, *pomost2;
int passengers_on_boat1 = 0, passengers_on_boat2 = 0;

volatile sig_atomic_t running = 1;

void cleanup(int signum) {
    printf("\nZwalnianie zasobów sternika...\n");

    if (pomost1 != NULL && shmdt(pomost1) == -1) {
        perror("Nie można odłączyć pamięci współdzielonej dla pomostu 1");
    }
    if (pomost2 != NULL && shmdt(pomost2) == -1) {
        perror("Nie można odłączyć pamięci współdzielonej dla pomostu 2");
    }

    if (shmctl(shmid_pomost1, IPC_RMID, NULL) == -1) {
        perror("Nie można usunąć pamięci współdzielonej dla pomostu 1");
    }
    if (shmctl(shmid_pomost2, IPC_RMID, NULL) == -1) {
        perror("Nie można usunąć pamięci współdzielonej dla pomostu 2");
    }

    exit(0);
}

void start_trip(const char *boat_name, struct pomost_state *pomost, int *passengers_on_boat) {
    if (*passengers_on_boat == 0) {
        printf("[%s] Sternik: Statek jest pusty, rejs nie może się rozpocząć.\n", boat_name);
        return;
    }

    printf("[%s] Sternik: Rozpoczynam rejs. Liczba pasażerów na statku: %d.\n", boat_name, *passengers_on_boat);

    pomost->direction = 0; // Zablokuj pomost na czas rejsu
    sleep(TRIP_DURATION);

    printf("[%s] Sternik: Rejs zakończony. Pasażerowie mogą schodzić.\n", boat_name);
    pomost->direction = -1; // Zezwól na schodzenie pasażerów

    *passengers_on_boat = 0;
}

void listen_to_boat(const char *fifo_path, const char *boat_name, struct pomost_state *pomost, int *passengers_on_boat) {
    char buffer[128];
    int fifo_fd = open(fifo_path, O_RDONLY | O_NONBLOCK);
    if (fifo_fd == -1) {
        perror("Nie można otworzyć FIFO dla sternika");
        return;
    }

    while (read(fifo_fd, buffer, sizeof(buffer)) > 0) {
        buffer[strcspn(buffer, "\n")] = '\0'; // Usuń nową linię
        if (strcmp(buffer, "Statek 1 jest pełny. Rozpocznij rejs.") == 0 ||
            strcmp(buffer, "Statek 2 jest pełny. Rozpocznij rejs.") == 0) {
            printf("[%s] Sternik: Otrzymano informację o pełnym statku. Rozpoczynam rejs.\n", boat_name);
            start_trip(boat_name, pomost, passengers_on_boat);
        } else {
            printf("[%s] Sternik: Otrzymano nieznany komunikat: %s\n", boat_name, buffer);
        }
    }

    close(fifo_fd);
}

int main() {
    signal(SIGINT, cleanup);
    signal(SIGTERM, cleanup);

    // Tworzenie pomostów (pamięć współdzielona)
    shmid_pomost1 = shmget(SHM_KEY_POMOST1, sizeof(struct pomost_state), IPC_CREAT | 0666);
    shmid_pomost2 = shmget(SHM_KEY_POMOST2, sizeof(struct pomost_state), IPC_CREAT | 0666);
    if (shmid_pomost1 == -1 || shmid_pomost2 == -1) {
        perror("Nie można utworzyć pamięci współdzielonej");
        exit(1);
    }

    pomost1 = (struct pomost_state *)shmat(shmid_pomost1, NULL, 0);
    pomost2 = (struct pomost_state *)shmat(shmid_pomost2, NULL, 0);
    if (pomost1 == (void *)-1 || pomost2 == (void *)-1) {
        perror("Nie można przyłączyć pamięci współdzielonej");
        cleanup(0);
    }

    pomost1->passengers_on_bridge = 0;
    pomost1->direction = 1;
    pomost2->passengers_on_bridge = 0;
    pomost2->direction = 1;

    // Tworzenie FIFO dla sternika
    if (mkfifo(FIFO_STERNIK1, 0666) == -1 && errno != EEXIST) {
        perror("Nie można utworzyć FIFO dla sternika 1");
        cleanup(0);
    }
    if (mkfifo(FIFO_STERNIK2, 0666) == -1 && errno != EEXIST) {
        perror("Nie można utworzyć FIFO dla sternika 2");
        cleanup(0);
    }

    printf("Sternik nasłuchuje komunikatów od statków.\n");

    while (running) {
        // Nasłuchiwanie komunikatów od statków
        listen_to_boat(FIFO_STERNIK1, "Statek 1", pomost1, &passengers_on_boat1);
        listen_to_boat(FIFO_STERNIK2, "Statek 2", pomost2, &passengers_on_boat2);

        sleep(1);
    }

    return 0;
}
