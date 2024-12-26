#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <time.h>

#define MSG_KEY 1234

struct msgbuf {
    long mtype;
    char mtext[100];
};

int main() {
    int msqid = msgget(MSG_KEY, IPC_CREAT | 0666);
    if (msqid == -1) {
        perror("Nie można utworzyć kolejki komunikatów");
        exit(1);
    }

    srand(time(NULL) ^ getpid());
    int wiek = rand() % 80 + 1;

    struct msgbuf msg;
    msg.mtype = 1;
    snprintf(msg.mtext, sizeof(msg.mtext), "Pasażer (wiek: %d, PID: %d)", wiek, getpid());

    if (msgsnd(msqid, &msg, sizeof(msg.mtext), 0) == -1) {
        perror("Nie można wysłać wiadomości");
        exit(1);
    }

    printf("Pasażer (wiek: %d, PID: %d) zgłosił się do kasjera\n", wiek, getpid());
    sleep(1); // Symulacja obsługi
    printf("Pasażer (wiek: %d, PID: %d) zakończył działanie\n", wiek, getpid());

    return 0;
}
