#include <stdio.h>
#include <stdlib.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <unistd.h>
#include <semaphore.h>
#include <fcntl.h>
#include <time.h>
#include "common.h"

int main() {
    int msq_depart = msgget(MSG_KEY_DEPART, 0666 | IPC_CREAT);
    if(msq_depart == -1) {
        perror("msgget (depart) w sternik");
        exit(EXIT_FAILURE);
    }
    
    int shmid = shmget(SHM_KEY_BOATS, sizeof(boats_shm_t), 0666 | IPC_CREAT);
    if(shmid == -1) {
        perror("shmget (boats) w sternik");
        exit(EXIT_FAILURE);
    }
    boats_shm_t *boats = (boats_shm_t *) shmat(shmid, NULL, 0);
    if(boats == (void *) -1) {
        perror("shmat (boats) w sternik");
        exit(EXIT_FAILURE);
    }
    
    sem_t *sem_pomost = sem_open("/sem_pomost", O_CREAT, 0666, POMOST_CAPACITY);
    if(sem_pomost == SEM_FAILED) {
        perror("sem_open (pomost) w sternik");
        exit(EXIT_FAILURE);
    }
    
    while(1) {
        sleep(2);
        time_t now = time(NULL);
        if(boats->boat1_boarding_open && boats->occupancy_boat1 > 0) {
            if((now - boats->boat1_start_time) >= MAX_WAIT) {
                boats->boat1_boarding_open = 0;
                boats->boat1_in_trip = 1;
                depart_msg dmsg; dmsg.mtype = 1;
                if(msgsnd(msq_depart, &dmsg, sizeof(dmsg)-sizeof(long), 0) == -1)
                    perror("msgsnd (depart statek 1) w sternik");
                else
                    printf("[Sternik] (MAX_WAIT) Zatwierdził odpłynięcie statku nr 1.\n");
            }
        }
        if(boats->boat2_boarding_open && boats->occupancy_boat2 > 0) {
            if((now - boats->boat2_start_time) >= MAX_WAIT) {
                boats->boat2_boarding_open = 0;
                boats->boat2_in_trip = 1;
                depart_msg dmsg; dmsg.mtype = 2;
                if(msgsnd(msq_depart, &dmsg, sizeof(dmsg)-sizeof(long), 0) == -1)
                    perror("msgsnd (depart statek 2) w sternik");
                else
                    printf("[Sternik] (MAX_WAIT) Zatwierdził odpłynięcie statku nr 2.\n");
            }
        }
    }
    
    shmdt(boats);
    sem_close(sem_pomost);
    return 0;
}
