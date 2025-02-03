#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include "common.h"

int main(int argc, char *argv[]) {
    if(argc != 3) {
        fprintf(stderr, "Usage: %s <boat1_pid> <boat2_pid>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    pid_t boat1_pid = atoi(argv[1]);
    pid_t boat2_pid = atoi(argv[2]);
    srand(time(NULL) ^ getpid());
    
    while(1) {
        int wait_time = (rand() % 21) + 10; // 10-30 sekund
        sleep(wait_time);
        int choice = rand() % 3;
        if(choice == 0) {
            kill(boat1_pid, SIGUSR1);
            printf("[Policja] Wysłano sygnał do statku 1 (przerwanie rejsu).\n");
        } else if(choice == 1) {
            kill(boat2_pid, SIGUSR2);
            printf("[Policja] Wysłano sygnał do statku 2 (przerwanie rejsu).\n");
        } else {
            kill(boat1_pid, SIGUSR1);
            kill(boat2_pid, SIGUSR2);
            printf("[Policja] Wysłano sygnały do obu statków (przerwanie rejsu).\n");
        }
        fflush(stdout);
    }
    
    return 0;
}
