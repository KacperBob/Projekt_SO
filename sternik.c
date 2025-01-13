#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <signal.h>

#define SHM_KEY 1234
#define SEM_KEY 5678
#define BOAT1_CAPACITY 10
#define BOAT2_CAPACITY 8
#define BRIDGE_CAPACITY 5
#define T1 10 // Czas rejsu łodzi 1 w sekundach
#define T2 15 // Czas rejsu łodzi 2 w sekundach

struct shared_state {
    int passengers_on_boat1;
    int passengers_on_boat2;
    int passengers_on_bridge;
    int bridge_direction;
    int max_bridge_capacity;
    int boat1_ready;
    int boat2_ready;
};

int shmid, semid;
struct shared_state *state;

void cleanup(int signum) {
    printf("Zwalnianie zasobów sternika...\n");
    if (shmdt(state) == -1) perror("Nie można odłączyć pamięci współdzielonej");
    if (shmctl(shmid, IPC_RMID, NULL) == -1) perror("Nie można usunąć pamięci współdzielonej");
    if (semctl(semid, 0, IPC_RMID) == -1) perror("Nie można usunąć semaforów");
    exit(0);
}

void semaphore_operation(int op) {
    struct sembuf sops = {0, op, 0};
    if (semop(semid, &sops, 1) == -1) {
        perror("Operacja semafora nie powiodła się");
        exit(1);
    }
}

void manage_boat(int *boat_passengers, int capacity, int time, const char *boat_name) {
    printf("%s: Oczekiwanie na pusty pomost lub pasażerów...\n", boat_name);

    int idle_time = 0; // Licznik bezczynności
    while (1) {
        // Sprawdzanie warunków: brak pasażerów na pomoście i załadunek
        if (state->passengers_on_bridge == 0 && *boat_passengers > 0) {
            break; // Pomost pusty i statek załadowany
        }

        // Monitorowanie czasu bezczynności (20 minut symulowanego czasu)
        sleep(1); // 1 sekunda = 1 minuta symulowanego czasu
        idle_time++;

        if (idle_time >= 20) {
            printf("%s: Minęło 20 minut bez pasażerów. Odpływam!\n", boat_name);
            break;
        }
    }

    printf("%s: Odpływam!\n", boat_name);
    state->bridge_direction = 0; // Blokowanie ruchu na pomoście
    state->boat1_ready = 0;

    sleep(time); // Symulacja rejsu

    printf("%s: Wróciłem z rejsu.\n", boat_name);
    *boat_passengers = 0; // Reset liczby pasażerów na łodzi
    state->boat1_ready = 1; // Gotowość do załadunku
    state->bridge_direction = 1; // Zezwolenie na wchodzenie
}

int main() {
    signal(SIGINT, cleanup);

    // Inicjalizacja pamięci współdzielonej
    shmid = shmget(SHM_KEY, sizeof(struct shared_state), IPC_CREAT | 0666);
    state = (struct shared_state *)shmat(shmid, NULL, 0);
    state->passengers_on_boat1 = 0;
    state->passengers_on_boat2 = 0;
    state->passengers_on_bridge = 0;
    state->bridge_direction = 0;
    state->max_bridge_capacity = BRIDGE_CAPACITY;
    state->boat1_ready = 1;
    state->boat2_ready = 1;

    // Inicjalizacja semaforów
    semid = semget(SEM_KEY, 1, IPC_CREAT | 0666);
    semctl(semid, 0, SETVAL, 1);

    while (1) {
        if (state->boat1_ready) manage_boat(&state->passengers_on_boat1, BOAT1_CAPACITY, T1, "Łódź 1");
        if (state->boat2_ready) manage_boat(&state->passengers_on_boat2, BOAT2_CAPACITY, T2, "Łódź 2");
        sleep(1);
    }

    return 0;
}
