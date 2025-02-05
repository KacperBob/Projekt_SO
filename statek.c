#include <stdio.h>
#include <stdlib.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <semaphore.h>
#include "common.h"

volatile sig_atomic_t cancel_trip = 0;
void sig_handler(int signo) { cancel_trip = 1; }

int main(int argc, char *argv[]) {
    if(argc != 2) {
        fprintf(stderr, "Usage: %s <boat_number>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    int boat_number = atoi(argv[1]);
    int trip_duration = (boat_number == 1) ? T1 : T2;
    
    if(boat_number == 1)
        signal(SIGUSR1, sig_handler);
    else if(boat_number == 2)
        signal(SIGUSR2, sig_handler);
    
    int msq_depart = msgget(MSG_KEY_DEPART, 0666 | IPC_CREAT);
    if(msq_depart == -1) {
        perror("msgget (depart) in statek");
        exit(EXIT_FAILURE);
    }
    
    int shmid = shmget(SHM_KEY_BOATS, sizeof(boats_shm_t), 0666 | IPC_CREAT);
    if(shmid == -1) {
        perror("shmget (boats) in statek");
        exit(EXIT_FAILURE);
    }
    boats_shm_t *boats = (boats_shm_t *) shmat(shmid, NULL, 0);
    if(boats == (void *) -1) {
        perror("shmat (boats) in statek");
        exit(EXIT_FAILURE);
    }
    
    srand(time(NULL) ^ getpid());
    
    while(1) {
        depart_msg dmsg;
        if(msgrcv(msq_depart, &dmsg, sizeof(dmsg) - sizeof(long), boat_number, 0) == -1) {
            perror("msgrcv (depart) in statek");
            continue;
        }
        
        if(cancel_trip) {
            printf("[Statek] Odebrano sygnał od Policji – statek %d nie wypływa, pasażerowie opuszczają łódź.\n", boat_number);
            sem_wait(&boats->occupancy_sem);
            if(boat_number == 1) {
                boats->occupancy_boat1 = 0;
                boats->boat1_boarding_open = 1;
                boats->boat1_in_trip = 0;
                boats->boat1_start_time = 0;
            } else {
                boats->occupancy_boat2 = 0;
                boats->boat2_boarding_open = 1;
                boats->boat2_in_trip = 0;
                boats->boat2_start_time = 0;
            }
            sem_post(&boats->occupancy_sem);
            cancel_trip = 0;
            continue;
        }
        
        /* Zabezpieczamy dostęp do occupancy */
        sem_wait(&boats->occupancy_sem);
        if(boat_number == 1) {
            boats->boat1_in_trip = 1;
            boats->boat1_boarding_open = 0;
        } else {
            boats->boat2_in_trip = 1;
            boats->boat2_boarding_open = 0;
        }
        int occupancy = (boat_number == 1) ? boats->occupancy_boat1 : boats->occupancy_boat2;
        sem_post(&boats->occupancy_sem);
        
        printf("[Statek] Odpływa statek nr %d z %d pasażerami.\n", boat_number, occupancy);
        sleep(trip_duration);
        
        /* Po rejsie wysyłamy komunikat "trip complete" do każdego pasażera */
        sem_wait(&boats->occupancy_sem);
        int msq_trip = msgget(MSG_KEY_TRIP, 0666 | IPC_CREAT);
        if(msq_trip == -1) {
            perror("msgget (trip complete) in statek");
        } else {
            if(boat_number == 1) {
                for (int i = 0; i < boats->occupancy_boat1; i++) {
                    trip_complete_msg tmsg;
                    tmsg.mtype = boats->boat1_pids[i];
                    if(msgsnd(msq_trip, &tmsg, sizeof(tmsg) - sizeof(long), 0) == -1) {
                        perror("msgsnd (trip complete) in statek");
                    }
                }
                boats->occupancy_boat1 = 0;
                boats->boat1_boarding_open = 1;
                boats->boat1_in_trip = 0;
                boats->boat1_start_time = 0;
            } else {
                for (int i = 0; i < boats->occupancy_boat2; i++) {
                    trip_complete_msg tmsg;
                    tmsg.mtype = boats->boat2_pids[i];
                    if(msgsnd(msq_trip, &tmsg, sizeof(tmsg) - sizeof(long), 0) == -1) {
                        perror("msgsnd (trip complete) in statek");
                    }
                }
                boats->occupancy_boat2 = 0;
                boats->boat2_boarding_open = 1;
                boats->boat2_in_trip = 0;
                boats->boat2_start_time = 0;
            }
        }
        sem_post(&boats->occupancy_sem);
        
        printf("[Statek] Statek nr %d przybył. Wszystkie miejsca są teraz wolne (%d miejsc dostępnych).\n",
               boat_number, (boat_number == 1) ? BOAT1_CAPACITY : BOAT2_CAPACITY);
    }
    
    shmdt(boats);
    return 0;
}
