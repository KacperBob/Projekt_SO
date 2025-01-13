#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#define MSG_KEY 1234

struct msgbuf {
    long mtype;          // Typ wiadomości
    char mtext[100];     // Treść wiadomości
};

int msqid = -1;

void cleanup(int signum) {
    (void)signum;
    if (msqid != -1) {
        msgctl(msqid, IPC_RMID, NULL);
    }
    exit(0);
}

int should_generate_passenger() {
    return (rand() % 100) < 20; // 20% szans na pojawienie się pasażera
}

void send_passenger(int msqid, int age, pid_t pid) {
    struct msgbuf msg;
    msg.mtype = 1; // Typ dla pojedynczego pasażera
    snprintf(msg.mtext, sizeof(msg.mtext), "Pasażer (wiek: %d, PID: %d)", age, pid);

    if (msgsnd(msqid, &msg, sizeof(msg.mtext), 0) == -1) {
        perror("Nie można wysłać wiadomości");
    } else {
        printf("Wysłano wiadomość: %s\n", msg.mtext);
    }
}

int main() {
    struct sigaction sa;
    sa.sa_handler = cleanup;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    srand(time(NULL));

    msqid = msgget(MSG_KEY, IPC_CREAT | 0666);
    if (msqid == -1) {
        perror("Nie można otworzyć kolejki komunikatów");
        exit(1);
    }

    while (1) {
        if (should_generate_passenger()) {
            int age = rand() % 56 + 15; // Wiek: 15–70
            send_passenger(msqid, age, getpid());
        }
        sleep(1);
    }

    cleanup(0);
    return 0;
}
