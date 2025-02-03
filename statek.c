#include <stdio.h>
#include <stdlib.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
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
        perror("msgget (depart) w statek");
        exit(EXIT_FAILURE);
    }
    
    int shmid = shmget(SHM_KEY_BOATS, sizeof(boats_shm_t), 0666 | IPC_CREAT);
    if(shmid == -1) {
        perror("shmget (boats) w statek");
        exit(EXIT_FAILURE);
    }
    boats_shm_t *boats = (boats_shm_t *) shmat(shmid, NULL, 0);
    if(boats == (void *) -1) {
        perror("shmat (boats) w statek");
        exit(EXIT_FAILURE);
    }
    
    while(1) {
        depart_msg dmsg;
        if(msgrcv(msq_depart, &dmsg, sizeof(dmsg)-sizeof(long), boat_number, 0) == -1) {
            perror("msgrcv (depart) w statek");
            continue;
        }
        
        if(cancel_trip) {
            printf("[Statek] Odebrano sygnał od Policji – statek %d nie wypływa, pasażerowie opuszczają statek.\n", boat_number);
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
            cancel_trip = 0;
            continue;
        }
        
        int occupancy = (boat_number == 1) ? boats->occupancy_boat1 : boats->occupancy_boat2;
        printf("[Statek] Odpływa statek nr %d z %d pasażerami.\n", boat_number, occupancy);
        
        sleep(trip_duration);  /* Symulacja rejsu */
        
        /* Przed resetowaniem, zapamiętujemy liczbę pasażerów na statku */
        int num_passengers = occupancy;
        
        /* Resetujemy stan statku */
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
        
        printf("[Statek] Statek nr %d przybył. Wszystkie miejsca są teraz wolne (%d miejsc dostępnych).\n",
               boat_number, (boat_number == 1) ? BOAT1_CAPACITY : BOAT2_CAPACITY);
        
        /* Dla każdego pasażera, który był na pokładzie, z 3% szansą uruchamiamy proces powracającego pasażera */
        for (int i = 0; i < num_passengers; i++) {
            if (rand() % 100 < 3) {  /* 3% szans */
                pid_t pid = fork();
                if (pid < 0) {
                    perror("fork (second trip)");
                } else if (pid == 0) {
                    printf("[Statek] Pasażer %d chce płynąć drugi raz.\n", getpid());
                    fflush(stdout);
                    /* Uruchamiamy nowy proces pasazer z argumentem "second" */
                    execl("./pasazer", "pasazer", "second", NULL);
                    perror("execl (pasazer second)");
                    exit(EXIT_FAILURE);
                }
            }
        }
    }
    
    shmdt(boats);
    return 0;
}
