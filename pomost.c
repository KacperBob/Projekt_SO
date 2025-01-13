#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <signal.h>
#include <errno.h>
#include <sys/wait.h>

#define SHM_KEY1 7890
#define SHM_KEY2 7891
#define SEM_KEY1 4567
#define SEM_KEY2 4568
#define MAX_PASSENGERS1 8
#define MAX_PASSENGERS2 5

struct pomost_state {
    int passengers_on_bridge;
    int direction; // 1 for boarding, -1 for leaving, 0 for idle
};

int shmid1, shmid2;
int semid1, semid2;
struct pomost_state *pomost1, *pomost2;

void cleanup(int signum) {
    printf("\nZwalnianie zasobów pomostów...\n");

    if (pomost1 != NULL && shmdt(pomost1) == -1) {
        perror("Nie można odłączyć pamięci współdzielonej dla pomostu 1");
    }
    if (pomost2 != NULL && shmdt(pomost2) == -1) {
        perror("Nie można odłączyć pamięci współdzielonej dla pomostu 2");
    }

    if (shmctl(shmid1, IPC_RMID, NULL) == -1) {
        perror("Nie można usunąć pamięci współdzielonej dla pomostu 1");
    }
    if (shmctl(shmid2, IPC_RMID, NULL) == -1) {
        perror("Nie można usunąć pamięci współdzielonej dla pomostu 2");
    }

    if (semctl(semid1, 0, IPC_RMID) == -1) {
        perror("Nie można usunąć semaforów dla pomostu 1");
    }
    if (semctl(semid2, 0, IPC_RMID) == -1) {
        perror("Nie można usunąć semaforów dla pomostu 2");
    }

    exit(0);
}

void handle_sigchld(int signum) {
    while (waitpid(-1, NULL, WNOHANG) > 0) {
        // Zbieranie zakończonych procesów potomnych
    }
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

void handle_pomost(struct pomost_state *pomost, int semid, int max_passengers, const char *pomost_name, int *last_direction) {
    semaphore_operation(semid, 0, -1); // Wejście do sekcji krytycznej

    // Wyświetl komunikat tylko, gdy zmienia się stan direction
    if (pomost->direction != *last_direction) {
        if (pomost->direction == 0) {
            printf("%s: Pomost gotowy przyjąć pasażerów na statek.\n", pomost_name);
        } else if (pomost->direction == -1) {
            printf("%s: Pomost gotowy do zejścia pasażerów ze statku.\n", pomost_name);
        }
        *last_direction = pomost->direction;
    }

    if (pomost->direction == 1) {
        if (pomost->passengers_on_bridge < max_passengers) {
            pomost->passengers_on_bridge++;
            printf("Pasażer (PID: %d) wszedł na %s. Liczba pasażerów: %d.\n", getpid(), pomost_name, pomost->passengers_on_bridge);
        } else {
            printf("%s pełny. Oczekiwanie na zejście pasażerów.\n", pomost_name);
        }
    } else if (pomost->direction == -1) {
        if (pomost->passengers_on_bridge > 0) {
            printf("Pasażer (PID: %d) zszedł z %s. Liczba pasażerów: %d.\n", getpid(), pomost_name, pomost->passengers_on_bridge);
            pomost->passengers_on_bridge--;
        } else {
            pomost->direction = 0;
        }
    }

    semaphore_operation(semid, 0, 1); // Wyjście z sekcji krytycznej
}

int main() {
    signal(SIGINT, cleanup);
    signal(SIGTERM, cleanup);
    signal(SIGCHLD, handle_sigchld);

    shmid1 = shmget(SHM_KEY1, sizeof(struct pomost_state), IPC_CREAT | 0666);
    shmid2 = shmget(SHM_KEY2, sizeof(struct pomost_state), IPC_CREAT | 0666);
    if (shmid1 == -1 || shmid2 == -1) {
        perror("Nie można utworzyć pamięci współdzielonej");
        exit(1);
    }

    pomost1 = (struct pomost_state *)shmat(shmid1, NULL, 0);
    pomost2 = (struct pomost_state *)shmat(shmid2, NULL, 0);
    if (pomost1 == (void *)-1 || pomost2 == (void *)-1) {
        perror("Nie można przyłączyć pamięci współdzielonej");
        cleanup(0);
    }

    pomost1->passengers_on_bridge = 0;
    pomost1->direction = 0;
    pomost2->passengers_on_bridge = 0;
    pomost2->direction = 0;

    semid1 = semget(SEM_KEY1, 1, IPC_CREAT | 0666);
    semid2 = semget(SEM_KEY2, 1, IPC_CREAT | 0666);
    if (semid1 == -1 || semid2 == -1) {
        perror("Nie można utworzyć semaforów");
        cleanup(0);
    }

    if (semctl(semid1, 0, SETVAL, 1) == -1 || semctl(semid2, 0, SETVAL, 1) == -1) {
        perror("Nie można ustawić wartości semaforów");
        cleanup(0);
    }

    printf("Pomosty są gotowe do obsługi pasażerów.\n");

    int last_direction1 = -2; // Przechowuje poprzedni stan kierunku dla Pomostu 1
    int last_direction2 = -2; // Przechowuje poprzedni stan kierunku dla Pomostu 2

    while (1) {
        handle_pomost(pomost1, semid1, MAX_PASSENGERS1, "Pomost 1", &last_direction1);
        handle_pomost(pomost2, semid2, MAX_PASSENGERS2, "Pomost 2", &last_direction2);
        sleep(1); // Symulacja czasu obsługi
    }

    cleanup(0);
    return 0;
}
