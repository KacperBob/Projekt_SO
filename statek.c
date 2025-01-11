#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <string.h>
#include <sys/wait.h>

#define SHM_KEY1 12345
#define SHM_KEY2 12346
#define SEM_KEY1 22345
#define SEM_KEY2 22346
#define FIFO_PATH1 "./fifo_boat1"
#define FIFO_PATH2 "./fifo_boat2"
#define BOAT1_CAPACITY 70
#define BOAT2_CAPACITY 50
#define TIMEOUT 20  // 20 minutes without passengers
#define TRIP_TIME 120 // 2 hours trip time

struct boat_state {
    int occupied_seats;
    pid_t passengers[70]; // Max capacity for boat 1
};

int shmid1, shmid2;
int semid1, semid2;
struct boat_state *boat1, *boat2;

void cleanup(int signum) {
    printf("\nZwalnianie zasobów...\n");

    // Zabijanie procesów potomnych
    printf("Zabijam procesy potomne...\n");
    kill(0, SIGTERM); // Zabij wszystkie procesy potomne w tej grupie procesów

    // Odłączanie i usuwanie pamięci współdzielonej dla łodzi 1
    if (boat1 != NULL && shmdt(boat1) == -1) {
        perror("Nie udało się odłączyć pamięci współdzielonej dla łodzi 1");
     } else {
        printf("Pamięć współdzielona dla łodzi 1 została odłączona.\n");
    }
    if (shmid1 != -1 && shmctl(shmid1, IPC_RMID, NULL) == -1) {
        perror("Nie udało się usunąć pamięci współdzielonej dla łodzi 1");
    } else {
        printf("Pamięć współdzielona dla łodzi 1 została usunięta.\n");
    }

    // Odłączanie i usuwanie pamięci współdzielonej dla łodzi 2
    if (boat2 != NULL && shmdt(boat2) == -1) {
        perror("Nie udało się odłączyć pamięci współdzielonej dla łodzi 2");
    } else {
        printf("Pamięć współdzielona dla łodzi 2 została odłączona.\n");
    }
    if (shmid2 != -1 && shmctl(shmid2, IPC_RMID, NULL) == -1) {
        perror("Nie udało się usunąć pamięci współdzielonej dla łodzi 2");
    } else {
        printf("Pamięć współdzielona dla łodzi 2 została usunięta.\n");
    }

    // Usuwanie semaforów
    if (semid1 != -1 && semctl(semid1, 0, IPC_RMID) == -1) {
        perror("Nie udało się usunąć semafora dla łodzi 1");
    } else {
        printf("Semafor dla łodzi 1 został usunięty.\n");
    }
    if (semid2 != -1 && semctl(semid2, 0, IPC_RMID) == -1) {
        perror("Nie udało się usunąć semafora dla łodzi 2");
    } else {
        printf("Semafor dla łodzi 2 został usunięty.\n");
    }

    // Usuwanie plików FIFO
    if (unlink(FIFO_PATH1) == 0) {
        printf("Plik FIFO dla łodzi 1 został usunięty.\n");
    } else {
        perror("Nie udało się usunąć pliku FIFO dla łodzi 1");
    }
    if (unlink(FIFO_PATH2) == 0) {
        printf("Plik FIFO dla łodzi 2 został usunięty.\n");
    } else {
        perror("Nie udało się usunąć pliku FIFO dla łodzi 2");
    }
    printf("Zasoby zostały zwolnione.\n");
    exit(0);
}

void handle_sigchld(int signum) {
    while (waitpid(-1, NULL, WNOHANG) > 0) {
        // Zbieranie zakończonych procesów
    }
}

void init_shared_memory() {
    shmid1 = shmget(SHM_KEY1, sizeof(struct boat_state), IPC_CREAT | 0666);
    shmid2 = shmget(SHM_KEY2, sizeof(struct boat_state), IPC_CREAT | 0666);
    if (shmid1 == -1 || shmid2 == -1) {
        perror("Nie udało się utworzyć pamięci współdzielonej");
        exit(1);
    }

    boat1 = (struct boat_state *)shmat(shmid1, NULL, 0);
    boat2 = (struct boat_state *)shmat(shmid2, NULL, 0);
    if (boat1 == (void *)-1 || boat2 == (void *)-1) {
        perror("Nie udało się podłączyć pamięci współdzielonej");
        cleanup(0);
    }

    memset(boat1, 0, sizeof(struct boat_state));
    memset(boat2, 0, sizeof(struct boat_state));
}

void init_semaphores() {
    semid1 = semget(SEM_KEY1, 1, IPC_CREAT | 0666);
    semid2 = semget(SEM_KEY2, 1, IPC_CREAT | 0666);
    if (semid1 == -1 || semid2 == -1) {
        perror("Nie udało się utworzyć semaforów");
        cleanup(0);
    }

    if (semctl(semid1, 0, SETVAL, 1) == -1 || semctl(semid2, 0, SETVAL, 1) == -1) {
        perror("Nie udało się zainicjować semaforów");
        cleanup(0);
    }
}

void create_fifos() {
    if (mkfifo(FIFO_PATH1, 0666) == -1 || mkfifo(FIFO_PATH2, 0666) == -1) {
        perror("Nie udało się utworzyć plików FIFO");
        cleanup(0);
    }
}

void handle_boat(const char *fifo_path, struct boat_state *boat, int capacity, int delay_between_trips) {
    int fifo_fd;
    char buffer[128];
    time_t last_passenger_time = time(NULL);

    printf("Łódź (%s) gotowa do obsługi.\n", fifo_path);

    while (1) {
        fifo_fd = open(fifo_path, O_RDONLY);
        if (fifo_fd == -1) {
            perror("Nie udało się otworzyć pliku FIFO");
            sleep(1); // Odczekaj chwilę i spróbuj ponownie
            continue;
        }

        printf("FIFO (%s) otwarte. Oczekiwanie na pasażerów...\n", fifo_path);

        while (read(fifo_fd, buffer, sizeof(buffer)) > 0) {
            pid_t passenger_pid;
            sscanf(buffer, "%d", &passenger_pid);

            printf("Odczytano pasażera o PID: %d\n", passenger_pid);

            if (boat->occupied_seats < capacity) {
                boat->passengers[boat->occupied_seats++] = passenger_pid;
                printf("Pasażer %d wszedł na pokład (%s). Liczba zajętych miejsc: %d/%d\n", passenger_pid, fifo_path, boat->occupied_seats, capacity);

                last_passenger_time = time(NULL);

                if (boat->occupied_seats == capacity) {
                    printf("Łódź (%s) jest w pełni załadowana. Odpływamy...\n", fifo_path);
                    break;
                }
            } else {
                printf("Pasażer %d nie został wpuszczony. Łódź (%s) jest pełna.\n", passenger_pid, fifo_path);
            }
        }

        close(fifo_fd);
        printf("Zamknięto FIFO (%s).\n", fifo_path);

        if (difftime(time(NULL), last_passenger_time) >= TIMEOUT * 60 || boat->occupied_seats == capacity) {
            printf("Łódź (%s) odpływa z %d pasażerami.\n", fifo_path, boat->occupied_seats);
            sleep(TRIP_TIME * 60); // Symulacja rejsu
            printf("Łódź (%s) wróciła z rejsu. Pasażerowie mogą opuszczać pokład.\n", fifo_path);

            memset(boat, 0, sizeof(struct boat_state));
            sleep(delay_between_trips * 60);
        }
    }
}

int main() {
    signal(SIGINT, cleanup);
    signal(SIGTERM, cleanup);
    signal(SIGCHLD, handle_sigchld);

    init_shared_memory();
    init_semaphores();
    create_fifos();

    if (fork() == 0) {
        // Proces dla łodzi 1
        handle_boat(FIFO_PATH1, boat1, BOAT1_CAPACITY, 30);
        exit(0);
    }

    if (fork() == 0) {
        // Proces dla łodzi 2
        handle_boat(FIFO_PATH2, boat2, BOAT2_CAPACITY, 30);
        exit(0);
    }

    while (1) {
        pause(); // Oczekuj na sygnały
    }

    return 0;
}
