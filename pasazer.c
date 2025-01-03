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

    // Szansa na grupę z dzieckiem (15%) lub pasażera powtarzającego wycieczkę (20%)
    int is_with_child = (rand() % 100) < 15;
    int is_repeating = (rand() % 100) < 20;
    int wiek = rand() % 80 + 1; // Wiek głównego pasażera

    struct msgbuf msg;
    msg.mtype = 1;

    if (is_with_child) {
        int child_age = rand() % 14 + 1; // Wiek dziecka (1-14)
        snprintf(msg.mtext, sizeof(msg.mtext), "Pasażer (wiek: %d, PID: %d, powtarzający: %d) z dzieckiem (wiek: %d)", wiek, getpid(), is_repeating, child_age);
        printf("Pasażer (wiek: %d, PID: %d, powtarzający: %d) z dzieckiem (wiek: %d) zgłasza się do kasjera\n", wiek, getpid(), is_repeating, child_age);
    } else {
        snprintf(msg.mtext, sizeof(msg.mtext), "Pasażer (wiek: %d, PID: %d, powtarzający: %d)", wiek, getpid(), is_repeating);
        printf("Pasażer (wiek: %d, PID: %d, powtarzający: %d) zgłasza się do kasjera\n", wiek, getpid(), is_repeating);
    }

    if (msgsnd(msqid, &msg, sizeof(msg.mtext), 0) == -1) {
        perror("Nie można wysłać wiadomości");
        exit(1);
    }

    sleep(1); // Symulacja obsługi
    printf("Pasażer (wiek: %d, PID: %d) zakończył działanie\n", wiek, getpid());

    return 0;
}
