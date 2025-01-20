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
#define SEM_KEY_POMOST1 4567
#define SEM_KEY_POMOST2 4568
#define FIFO_BOAT1 "fifo_boat1"
#define FIFO_BOAT2 "fifo_boat2"
#define MAX_PASSENGERS_BOAT1 50
#define MAX_PASSENGERS_BOAT2 40
#define TRIP_DURATION 5 // Czas trwania rejsu w sekundach
#define TIME_LIMIT 20 // Limit czasu oczekiwania na pasażera w sekundach

struct pomost_state {
    int passengers_on_bridge;
    int direction; // 1 for boarding, -1 for leaving, 0 for idle
};

int shmid_pomost1, shmid_pomost2;
int semid_pomost1, semid_pomost2;
struct pomost_state *pomost1, *pomost2;
int passengers_on_boat1 = 0, passengers_on_boat2 = 0;
time_t last_passenger_time_boat1, last_passenger_time_boat2;

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

    if (semctl(semid_pomost1, 0, IPC_RMID) == -1) {
        perror("Nie można usunąć semafora dla pomostu 1");
    }
    if (semctl(semid_pomost2, 0, IPC_RMID) == -1) {
        perror("Nie można usunąć semafora dla pomostu 2");
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

void start_trip(struct pomost_state *pomost, int *passengers_on_boat, int semid, const char *pomost_name) {
    if (*passengers_on_boat == 0) {
        printf("[%s] Sternik: Statek jest pusty, rejs nie może się rozpocząć.\n", pomost_name);
        return;
    }

    printf("[%s] Sternik: Rozpoczynam rejs. Liczba pasażerów na statku: %d.\n", pomost_name, *passengers_on_boat);

    semaphore_operation(semid, 0, -1);
    while (pomost->passengers_on_bridge > 0) {
        printf("[%s] Sternik: Oczekiwanie, aż pomost się opróżni.\n", pomost_name);
        sleep(1);
    }
    pomost->direction = 0;

    sleep(TRIP_DURATION);

    printf("[%s] Sternik: Rejs zakończony. Pasażerowie mogą schodzić.\n", pomost_name);
    pomost->direction = -1;
    semaphore_operation(semid, 0, 1);

    *passengers_on_boat = 0;
}

void control_pomost_and_boat(struct pomost_state *pomost, int *passengers_on_boat, int semid, int max_passengers, const char *fifo_path, const char *pomost_name, time_t *last_passenger_time) {
    semaphore_operation(semid, 0, -1);

    char buffer[256];
    int fifo_fd = open(fifo_path, O_RDONLY | O_NONBLOCK);
    if (fifo_fd == -1) {
        perror("Nie można otworzyć FIFO dla statku");
        semaphore_operation(semid, 0, 1);
        return;
    }

    while (read(fifo_fd, buffer, sizeof(buffer)) > 0) {
        if (pomost->direction == 1) {
            if (*passengers_on_boat < max_passengers) {
                (*passengers_on_boat)++;
                *last_passenger_time = time(NULL);
                printf("[%s] Sternik: Pasażer wsiada na statek. Liczba pasażerów na statku: %d.\n", pomost_name, *passengers_on_boat);
            } else {
                printf("[%s] Sternik: Statek pełny, pasażer musi poczekać.\n", pomost_name);
            }
        }
    }

    close(fifo_fd);
    semaphore_operation(semid, 0, 1);

    if (*passengers_on_boat == max_passengers || difftime(time(NULL), *last_passenger_time) >= TIME_LIMIT) {
        start_trip(pomost, passengers_on_boat, semid, pomost_name);
    }
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
    pomost1->direction = 1;
    pomost2->passengers_on_bridge = 0;
    pomost2->direction = 1;

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

    passengers_on_boat1 = 0;
    passengers_on_boat2 = 0;
    last_passenger_time_boat1 = time(NULL);
    last_passenger_time_boat2 = time(NULL);

    printf("Sternik kontroluje pomosty i statki.\n");

    while (1) {
        control_pomost_and_boat(pomost1, &passengers_on_boat1, semid_pomost1, MAX_PASSENGERS_BOAT1, FIFO_BOAT1, "Pomost 1", &last_passenger_time_boat1);
        control_pomost_and_boat(pomost2, &passengers_on_boat2, semid_pomost2, MAX_PASSENGERS_BOAT2, FIFO_BOAT2, "Pomost 2", &last_passenger_time_boat2);
        sleep(1);
    }

    return 0;
}
