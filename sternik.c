#include <stdio.h>
#include <stdlib.h>
#include <sys/msg.h>
#include <unistd.h>
#include "common.h"

int main() {
    int msq_depart = msgget(MSG_KEY_DEPART, 0666 | IPC_CREAT);
    if (msq_depart == -1) {
        perror("msgget (depart) in sternik");
        exit(EXIT_FAILURE);
    }
    
    while (1) {
        sleep(MAX_WAIT);
        depart_msg dmsg;
        dmsg.mtype = 1;  // dla statku 1
        if (msgsnd(msq_depart, &dmsg, sizeof(dmsg) - sizeof(long), 0) == -1) {
            perror("msgsnd (depart) for statek 1 in sternik");
        }
        dmsg.mtype = 2;  // dla statku 2
        if (msgsnd(msq_depart, &dmsg, sizeof(dmsg) - sizeof(long), 0) == -1) {
            perror("msgsnd (depart) for statek 2 in sternik");
        }
    }
    
    return 0;
}
