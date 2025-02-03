#include <stdio.h>
#include <stdlib.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <unistd.h>
#include "common.h"

int main() {
    int msq_ticket = msgget(MSG_KEY_TICKET, 0666 | IPC_CREAT);
    if(msq_ticket == -1) {
        perror("msgget (ticket) w kasjer");
        exit(EXIT_FAILURE);
    }
    
    int shmid = shmget(SHM_KEY_BOATS, sizeof(boats_shm_t), 0666 | IPC_CREAT);
    if(shmid == -1) {
        perror("shmget (boats) w kasjer");
        exit(EXIT_FAILURE);
    }
    boats_shm_t *boats = (boats_shm_t *) shmat(shmid, NULL, 0);
    if(boats == (void *) -1) {
        perror("shmat (boats) w kasjer");
        exit(EXIT_FAILURE);
    }
    
    while(1) {
        ticket_request_msg req;
        if(msgrcv(msq_ticket, &req, sizeof(req) - sizeof(long), 1, 0) == -1) {
            perror("msgrcv (ticket request) w kasjer");
            continue;
        }
        
        int boat_assigned, price;
        /* Jeśli pasażer przychodzi z opiekunem, to zawsze trafia na statek nr 2. */
        if(req.with_guardian == 1) {
            boat_assigned = 2;
            price = 10;
        } else {
            /* Pasażer samodzielny (pierwszy rejs) trafia na statek nr 1. */
            boat_assigned = 1;
            price = 10;
        }
        if(req.second_trip == 1) {
            price /= 2;
        }
        
        printf("[Kasjer] Obsłużył Pasażera: %d%s, Wiek: %d, Zapłacił: %d, wysłał go na statek nr %d (DRUGI REJS: %s).\n",
               req.pid,
               (req.with_guardian ? " (z opiekunem)" : ""),
               req.age,
               price,
               boat_assigned,
               (req.second_trip ? "TAK" : "NIE"));
        
        ticket_response_msg resp;
        resp.mtype = req.pid;
        resp.boat_assigned = boat_assigned;
        resp.price = price;
        
        if(msgsnd(msq_ticket, &resp, sizeof(resp) - sizeof(long), 0) == -1) {
            perror("msgsnd (ticket response) w kasjer");
        }
    }
    
    shmdt(boats);
    return 0;
}
