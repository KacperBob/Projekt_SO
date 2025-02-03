#include <stdio.h>
#include <stdlib.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <unistd.h>
#include <semaphore.h>
#include <fcntl.h>
#include <errno.h>
#include "common.h"

int main() {
    int msq_boarding = msgget(MSG_KEY_BOARDING, 0666 | IPC_CREAT);
    if(msq_boarding == -1) {
        perror("msgget (boarding) w pomost");
        exit(EXIT_FAILURE);
    }
    
    int shmid = shmget(SHM_KEY_BOATS, sizeof(boats_shm_t), 0666 | IPC_CREAT);
    if(shmid == -1) {
        perror("shmget (boats) w pomost");
        exit(EXIT_FAILURE);
    }
    boats_shm_t *boats = (boats_shm_t *) shmat(shmid, NULL, 0);
    if(boats == (void *) -1) {
        perror("shmat (boats) w pomost");
        exit(EXIT_FAILURE);
    }
    
    /* Inicjalizacja */
    boats->occupancy_boat1 = 0;
    boats->occupancy_boat2 = 0;
    boats->boat1_boarding_open = 1;
    boats->boat2_boarding_open = 1;
    boats->boat1_in_trip = 0;
    boats->boat2_in_trip = 0;
    boats->boat1_start_time = 0;
    boats->boat2_start_time = 0;
    
    sem_t *sem_pomost = sem_open("/sem_pomost", O_CREAT, 0666, POMOST_CAPACITY);
    if(sem_pomost == SEM_FAILED) {
        perror("sem_open (pomost) w pomost");
        exit(EXIT_FAILURE);
    }
    
    boarding_request_msg board_req;
    while(1) {
        int ret = msgrcv(msq_boarding, &board_req, sizeof(board_req)-sizeof(long), 0, IPC_NOWAIT);
        if(ret == -1) {
            if(errno == ENOMSG) { usleep(100000); continue; }
            else { perror("msgrcv (boarding request) w pomost"); continue; }
        }
        
        if(board_req.boat == 1) {
            if(boats->boat1_in_trip || !boats->boat1_boarding_open) {
                printf("[Pomost] Statek 1 jest zamknięty lub w rejsie, pasażer: %d nie może wejść.\n", board_req.pid);
                continue;
            }
            if(boats->occupancy_boat1 == 0)
                boats->boat1_start_time = time(NULL);
            if(boats->occupancy_boat1 < BOAT1_CAPACITY) {
                boats->occupancy_boat1++;
                int remaining = BOAT1_CAPACITY - boats->occupancy_boat1;
                printf("[Pomost] Pasażer: %d wszedł na statek 1. Pozostałych miejsc: %d\n", board_req.pid, remaining);
            } else {
                printf("[Pomost] Pasażer: %d nie może wejść na statek 1 – pełny.\n", board_req.pid);
            }
        } else if(board_req.boat == 2) {
            if(boats->boat2_in_trip || !boats->boat2_boarding_open) {
                printf("[Pomost] Statek 2 jest zamknięty lub w rejsie, pasażer: %d nie może wejść.\n", board_req.pid);
                continue;
            }
            if(boats->occupancy_boat2 == 0)
                boats->boat2_start_time = time(NULL);
            if(board_req.with_guardian == 1) {
                /* Pasażer z opiekunem zajmuje 2 miejsca */
                if(boats->occupancy_boat2 <= BOAT2_CAPACITY - 2) {
                    boats->occupancy_boat2 += 2;
                    int remaining = BOAT2_CAPACITY - boats->occupancy_boat2;
                    printf("[Pomost] Pasażer: %d (z opiekunem) wszedł na statek 2. Pozostałych miejsc: %d\n", board_req.pid, remaining);
                } else {
                    printf("[Pomost] Pasażer: %d (z opiekunem) nie może wejść na statek 2 – za mało miejsc.\n", board_req.pid);
                }
            } else {
                if(boats->occupancy_boat2 < BOAT2_CAPACITY) {
                    boats->occupancy_boat2++;
                    int remaining = BOAT2_CAPACITY - boats->occupancy_boat2;
                    printf("[Pomost] Pasażer: %d wszedł na statek 2. Pozostałych miejsc: %d\n", board_req.pid, remaining);
                } else {
                    printf("[Pomost] Pasażer: %d nie może wejść na statek 2 – pełny.\n", board_req.pid);
                }
            }
        }
        usleep(100000);
    }
    
    shmdt(boats);
    sem_close(sem_pomost);
    return 0;
}
