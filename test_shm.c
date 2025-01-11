#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <string.h>

#define SHM_KEY1 12345
#define SHM_KEY2 12346

struct boat_state {
    int occupied_seats;
    pid_t passengers[70]; // Max capacity for testing
};

int main() {
    int shmid1, shmid2;

    // Tworzenie pamięci współdzielonej
    shmid1 = shmget(SHM_KEY1, sizeof(struct boat_state), IPC_CREAT | 0666);
    if (shmid1 == -1) {
        perror("Nie udało się utworzyć pamięci SHM_KEY1");
        exit(1);
    }
    printf("Utworzono segment SHM_KEY1, shmid: %d\n", shmid1);

    shmid2 = shmget(SHM_KEY2, sizeof(struct boat_state), IPC_CREAT | 0666);
    if (shmid2 == -1) {
        perror("Nie udało się utworzyć pamięci SHM_KEY2");
        exit(1);
    }
    printf("Utworzono segment SHM_KEY2, shmid: %d\n", shmid2);

    // Odłączanie i usuwanie segmentów
    if (shmctl(shmid1, IPC_RMID, NULL) == -1) {
        perror("Nie udało się usunąć SHM_KEY1");
    } else {
        printf("Usunięto segment SHM_KEY1\n");
    }

    if (shmctl(shmid2, IPC_RMID, NULL) == -1) {
        perror("Nie udało się usunąć SHM_KEY2");
    } else {
        printf("Usunięto segment SHM_KEY2\n");
    }

    return 0;
}
