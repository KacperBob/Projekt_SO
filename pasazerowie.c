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

void send_passenger_with_dependant(int msqid, int parent_age, pid_t parent_pid, int dependant_age, pid_t dependant_pid) {
    struct msgbuf msg;
    msg.mtype = 2; // Typ dla pasażera z osobą zależną
    snprintf(msg.mtext, sizeof(msg.mtext),
             "Pojawił się pasażer z dzieckiem/seniorem PID rodzica: %d, wiek: %d, PID dziecka/seniora: %d, wiek: %d",
             parent_pid, parent_age, dependant_pid, dependant_age);

    if (msgsnd(msqid, &msg, sizeof(msg.mtext), 0) == -1) {
        perror("Nie można wysłać wiadomości");
    } else {
        printf("Wysłano wiadomość z osobą zależną: %s\n", msg.mtext);
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
        int parent_age = rand() % 56 + 15; // Wiek rodzica: 15–70
        pid_t parent_pid = getpid();

        pid_t dependant_pid = fork();
        if (dependant_pid == 0) { // Proces potomny
            int dependant_age;
            if (rand() % 2 == 0) {
                dependant_age = rand() % 14 + 1; // Wiek dziecka: 1–14
            } else {
                dependant_age = rand() % 30 + 71; // Wiek seniora: 71–100
            }

            send_passenger_with_dependant(msqid, parent_age, parent_pid, dependant_age, getpid());
            exit(0);
        } else if (dependant_pid > 0) {
            printf("Utworzono proces potomny: PID rodzica: %d, PID potomka: %d\n", parent_pid, dependant_pid);
        } else {
            perror("Nie można utworzyć procesu potomnego");
        }
        sleep(2);
    }

    cleanup(0);
    return 0;
}
