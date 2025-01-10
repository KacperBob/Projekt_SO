#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <signal.h>
#include <time.h>

#define SHM_KEY 5678
#define MSG_KEY 1234

struct msgbuf {
    long mtype;
    char mtext[100];
};

void cleanup(int signum) {
    printf("Proces pasażer (PID: %d) zakończony.\n", getpid());
    exit(0);
}

void send_message(int msqid, const char *message) {
    struct msgbuf msg;
    msg.mtype = 1;
    snprintf(msg.mtext, sizeof(msg.mtext), "%s", message);

    if (msgsnd(msqid, &msg, sizeof(msg.mtext), 0) == -1) {
        perror("Nie udało się wysłać wiadomości");
        exit(1);
    }
}

void get_simulated_time(char *buffer) {
    int shmid = shmget(SHM_KEY, sizeof(char) * 20, 0666);
    if (shmid == -1) {
        perror("Nie udało się połączyć z pamięcią współdzieloną");
        exit(1);
    }

    char *czas = shmat(shmid, NULL, 0);
    if (czas == (char *)-1) {
        perror("Nie udało się odczytać czasu");
        exit(1);
    }

    snprintf(buffer, 20, "%s", czas);

    if (shmdt(czas) == -1) {
        perror("Nie udało się odłączyć pamięci współdzielonej");
    }
}

int main() {
    signal(SIGINT, cleanup);
    signal(SIGTERM, cleanup);

    int msqid = msgget(MSG_KEY, 0666);
    if (msqid == -1) {
        perror("Nie udało się uzyskać dostępu do kolejki komunikatów");
        exit(1);
    }

    srand(time(NULL) ^ getpid());

    int age = (rand() % 100) + 1; // Wiek pasażera ograniczony do 100 lat
    char simulated_time[20];
    get_simulated_time(simulated_time);

    printf("[%s] Pojawił się klient (PID: %d, wiek: %d) i stanął w kolejce.\n", simulated_time, getpid(), age);

    char message[100];
    snprintf(message, sizeof(message), "Pasażer (wiek: %d, PID: %d)", age, getpid());
    send_message(msqid, message);

    exit(0); // Kończymy proces po wysłaniu wiadomości

    return 0;
}
