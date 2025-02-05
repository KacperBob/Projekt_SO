#include <stdio.h>
#include <stdlib.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <unistd.h>
#include <errno.h>
#include <semaphore.h>
#include "common.h"

int main() {
    int msq_boarding = msgget(MSG_KEY_BOARDING, 0666 | IPC_CREAT);
    if (msq_boarding == -1) {
        perror("msgget (boarding) in pomost");
        exit(EXIT_FAILURE);
    }
    
    int shmid = shmget(SHM_KEY_BOATS, sizeof(boats_shm_t), 0666 | IPC_CREAT);
    if (shmid == -1) {
        perror("shmget (boats) in pomost");
        exit(EXIT_FAILURE);
    }
    boats_shm_t *boats = (boats_shm_t *) shmat(shmid, NULL, 0);
    if (boats == (void *) -1) {
        perror("shmat (boats) in pomost");
        exit(EXIT_FAILURE);
    }
    
    boarding_request_msg board_req;
    while (1) {
        int ret = msgrcv(msq_boarding, &board_req, sizeof(board_req) - sizeof(long), 0, IPC_NOWAIT);
        if (ret == -1) {
            if (errno == ENOMSG) {
                usleep(100000);
                continue;
            } else {
                perror("msgrcv (boarding request) in pomost");
                continue;
            }
        }
        
        sem_wait(&boats->occupancy_sem);
        if (board_req.boat == 1) {
            if (boats->boat1_in_trip || !boats->boat1_boarding_open) {
                printf("[Pomost] Statek 1 jest zamknięty lub w rejsie, pasażer: %d nie może wejść.\n", board_req.pid);
                sem_post(&boats->occupancy_sem);
                continue;
            }
            if (boats->occupancy_boat1 < BOAT1_CAPACITY) {
                boats->boat1_pids[boats->occupancy_boat1] = board_req.pid;
                boats->occupancy_boat1++;
                int remaining = BOAT1_CAPACITY - boats->occupancy_boat1;
                printf("[Pomost] Pasażer: %d wszedł na statek 1. Pozostałych miejsc: %d\n", board_req.pid, remaining);
            } else {
                printf("[Pomost] Pasażer: %d nie może wejść na statek 1 – pełny.\n", board_req.pid);
            }
        } else if (board_req.boat == 2) {
            if (boats->boat2_in_trip || !boats->boat2_boarding_open) {
                printf("[Pomost] Statek 2 jest zamknięty lub w rejsie, pasażer: %d nie może wejść.\n", board_req.pid);
                sem_post(&boats->occupancy_sem);
                continue;
            }
            if (board_req.with_guardian == 1) {
                if (boats->occupancy_boat2 <= BOAT2_CAPACITY - 2) {
                    boats->boat2_pids[boats->occupancy_boat2] = board_req.pid;
                    boats->occupancy_boat2 += 2;
                    int remaining = BOAT2_CAPACITY - boats->occupancy_boat2;
                    printf("[Pomost] Pasażer: %d (z opiekunem) wszedł na statek 2. Pozostałych miejsc: %d\n", board_req.pid, remaining);
                } else {
                    printf("[Pomost] Pasażer: %d (z opiekunem) nie może wejść na statek 2 – za mało miejsc.\n", board_req.pid);
                }
            } else {
                if (boats->occupancy_boat2 < BOAT2_CAPACITY) {
                    boats->boat2_pids[boats->occupancy_boat2] = board_req.pid;
                    boats->occupancy_boat2++;
                    int remaining = BOAT2_CAPACITY - boats->occupancy_boat2;
                    printf("[Pomost] Pasażer: %d wszedł na statek 2. Pozostałych miejsc: %d\n", board_req.pid, remaining);
                } else {
                    printf("[Pomost] Pasażer: %d nie może wejść na statek 2 – pełny.\n", board_req.pid);
                }
            }
        }
        sem_post(&boats->occupancy_sem);
        usleep(100000);
    }
    
    shmdt(boats);
    return 0;
}
