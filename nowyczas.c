#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#define SHM_KEY 5678
#define SEKUNDY_W_GODZINIE 60

int shmid;
char *czas;

// Funkcja czyszcząca pamięć współdzieloną
void clean_up(int signum) {
    printf("\nPrzerywanie programu...\n");
    if (shmdt(czas) == -1) {
        perror("Nie można odłączyć pamięci współdzielonej");
    }
    if (shmctl(shmid, IPC_RMID, NULL) == -1) {
        perror("Nie można usunąć segmentu pamięci współdzielonej");
    }
    exit(0);
}

int main() {
    // Rejestrowanie obsługi sygnałów
    signal(SIGINT, clean_up);   // Obsługa Ctrl+C
    signal(SIGTERM, clean_up);  // Obsługa polecenia kill
    signal(SIGTSTP, clean_up);  // Obsługa Ctrl+Z

    // Tworzenie segmentu pamięci współdzielonej
    shmid = shmget(SHM_KEY, sizeof(char) * 20, IPC_CREAT | 0666);
    if (shmid == -1) {
        perror("Nie można utworzyć segmentu pamięci współdzielonej");
        exit(1);
    }

    // Podłączenie do pamięci współdzielonej
    czas = shmat(shmid, NULL, 0);
    if (czas == (char *)-1) {
        perror("Nie można przyłączyć pamięci współdzielonej");
        shmctl(shmid, IPC_RMID, NULL); // Usuwanie pamięci, jeśli podłączenie się nie powiodło
        exit(1);
    }

    // Zaczynamy symulację od godziny 6:00
    int godzina = 6, minuta = 0;

    // Symulacja czasu
    while (1) {
        snprintf(czas, 20, "%02d:%02d:00", godzina, minuta);
        fflush(stdout);

        minuta++;
        if (minuta >= 60) {
            minuta = 0;
            godzina++;
        }
        if (godzina >= 24) {
            godzina = 0;
        }

        usleep(1000000); // 1 sekunda rzeczywista = 1 minuta symulowanego czasu
    }

    return 0; // Kod nigdy tu nie dotrze z powodu nieskończonej pętli
}
