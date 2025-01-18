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

#define SHM_KEY_POMOST1 7890
#define SHM_KEY_POMOST2 7891
#define SEM_KEY_POMOST1 4567
#define SEM_KEY_POMOST2 4568
#define FIFO_BOAT1 "fifo_boat1"
#define FIFO_BOAT2 "fifo_boat2"
#define MAX_PASSENGERS_BOAT1 50
#define MAX_PASSENGERS_BOAT2 40

struct pomost_state {
    int passengers_on_bridge;
    int direction; // 1 for boarding, -1 for leaving, 0 for idle
};

int shmid_pomost1, shmid_pomost2;
int semid_pomost1, semid_pomost2;
struct pomost_state *pomost1, *pomost2;
int fifo_boat1, fifo_boat2;

void cleanup(int signum) {
    printf("\nZwalnianie zasobów sternika...\n");

    if (pomost1 != NULL && shmdt(pomost1) == -1) {
        perror("Nie można odłączyć pamięci współdzielonej dla pomostu 1");
    }
    if (pomost2 != NULL && shmdt(pomost2) == -1) {
        perror("Nie można odłączyć pamięci współdzielonej dla pomostu 2");
    }

    if (fifo_boat1 >= 0) {
        close(fifo_boat1);
    }
    if (fifo_boat2 >= 0) {
        close(fifo_boat2);
    }

    exit(0);
}

void semaphore_operation(int semid, int sem_num, int op) {
    struct sembuf sops;
    sops.sem_num = sem_num;
    sops.sem_op = op;
    sops.sem_flg = 0;

    if (semop(semid, &sops, 1) == -1) {
        perror("Operacja semafora się nie powiodła");
        exit(1);
    }
}

void control_pomost_and_boat(struct pomost_state *pomost, int semid, int max_passengers, const char *fifo_path, const char *pomost_name) {
    semaphore_operation(semid, 0, -1); // Wejście do sekcji krytycznej

    char buffer[256];
    int fifo_fd = open(fifo_path, O_RDONLY | O_NONBLOCK);
    if (fifo_fd == -1) {
        perror("Nie można otworzyć FIFO dla statku");
        semaphore_operation(semid, 0, 1);
        return;
    }

    while (read(fifo_fd, buffer, sizeof(buffer)) > 0) {
        printf("[%s] Sternik otrzymał: %s\n", pomost_name, buffer);
        if (pomost->direction == 1) {
            if (pomost->passengers_on_bridge < max_passengers) {
                pomost->passengers_on_bridge++;
                printf("[%s] Sternik: Pasażer wchodzi na pomost. Liczba pasażerów: %d.\n", pomost_name, pomost->passengers_on_bridge);
            } else {
                printf("[%s] Sternik: Pomost pełny, pasażer musi poczekać.\n", pomost_name);
            }
        } else if (pomost->direction == -1) {
            if (pomost->passengers_on_bridge > 0) {
                pomost->passengers_on_bridge--;
                printf("[%s] Sternik: Pasażer opuszcza pomost. Liczba pasażerów: %d.\n", pomost_name, pomost->passengers_on_bridge);
            }
        }
    }

    close(fifo_fd);
    semaphore_operation(semid, 0, 1); // Wyjście z sekcji krytycznej
}

int main() {
    signal(SIGINT, cleanup);
    signal(SIGTERM, cleanup);

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
    pomost1->direction = 1; // Domyślnie boarding
    pomost2->passengers_on_bridge = 0;
    pomost2->direction = 1; // Domyślnie boarding

    semid_pomost1 = semget(SEM_KEY_POMOST1, 1, IPC_CREAT | 0666);
    semid_pomost2 = semget(SEM_KEY_POMOST2, 1, IPC_CREAT | 0666);
    if (semid_pomost1 == -1 || semid_pomost2 == -1) {
        perror("Nie można utworzyć semaforów");
        cleanup(0);
    }

    if (semctl(semid_pomost1, 0, SETVAL, 1) == -1 || semctl(semid_pomost2, 0, SETVAL, 1) == -1) {
        perror("Nie można ustawić wartości semaforów");
        cleanup(0);
    }

    printf("Sternik kontroluje pomosty i statki.\n");

    while (1) {
        control_pomost_and_boat(pomost1, semid_pomost1, MAX_PASSENGERS_BOAT1, FIFO_BOAT1, "Pomost 1");
        control_pomost_and_boat(pomost2, semid_pomost2, MAX_PASSENGERS_BOAT2, FIFO_BOAT2, "Pomost 2");
        sleep(1); // Symulacja czasu przetwarzania
    }

    cleanup(0);
    return 0;
}
