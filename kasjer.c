#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <signal.h>
#include <string.h>

#define SHM_KEY 5678
#define MSG_KEY 1234

struct msgbuf {
    long mtype;
    char mtext[100];
};

int msqid = -1; // Kolejka komunikatów
int shmid = -1; // Segment pamięci współdzielonej
char *czas = NULL; // Wskaźnik do pamięci współdzielonej

// Funkcja czyszcząca zasoby
void cleanup(int signum) {
    printf("\nZwalnianie zasobów...\n");

    // Odłącz pamięć współdzieloną
    if (czas != NULL) {
        if (shmdt(czas) == -1) {
            perror("Nie można odłączyć pamięci współdzielonej");
        }
    }

    // Nie usuwaj kolejki komunikatów – tylko proces nadrzędny powinien to robić
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

int main() {
    // Rejestrowanie obsługi sygnałów
    signal(SIGINT, cleanup);
    signal(SIGTERM, cleanup);

    // Tworzenie kolejki komunikatów
    msqid = msgget(MSG_KEY, IPC_CREAT | 0666);
    if (msqid == -1) {
        perror("Nie można utworzyć kolejki komunikatów");
        exit(1);
    }

    struct msgbuf msg;
    char simulated_time[20];

    while (1) {
        // Odbieranie wiadomości z kolejki komunikatów
        if (msgrcv(msqid, &msg, sizeof(msg.mtext), 1, 0) == -1) {
            perror("Błąd podczas odbioru wiadomości");
            sleep(1); // Poczekaj przed ponowną próbą, aby nie przeciążać systemu
            continue;
        }

        // Pobieranie czasu symulowanego
        get_simulated_time(simulated_time);

        // Wyświetlanie komunikatu o obsłudze klienta
        printf("[%s] Kasjer obsłużył: %s\n", simulated_time, msg.mtext);
    }

    cleanup(0); // Dodatkowe czyszczenie przy zakończeniu
    return 0;
}
