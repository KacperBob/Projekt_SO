#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#define SHM_KEY 5678

int main() {
    int shmid = shmget(SHM_KEY, sizeof(char) * 20, 0666);
    if (shmid == -1) {
        perror("Nie można połączyć się z pamięcią współdzieloną");
        exit(1);
    }

    char *czas = shmat(shmid, NULL, 0);
    if (czas == (char *)-1) {
        perror("Nie można przyłączyć pamięci współdzielonej");
        exit(1);
    }

    printf("Czas z pamięci współdzielonej: %s\n", czas);

    shmdt(czas);
    return 0;
}
