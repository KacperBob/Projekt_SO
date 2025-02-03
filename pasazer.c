#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/msg.h>
#include <unistd.h>
#include <time.h>
#include "common.h"

int main(int argc, char *argv[]) {
    /* Jeśli program uruchomiono z argumentem "second", to jest to powracający pasażer */
    int is_second = 0;
    if (argc >= 2 && strcmp(argv[1], "second") == 0) {
        is_second = 1;
    }
    
    srand(time(NULL) ^ getpid());
    
    int msq_ticket = msgget(MSG_KEY_TICKET, 0666);
    if(msq_ticket == -1) {
        perror("msgget (ticket)");
        exit(EXIT_FAILURE);
    }
    
    ticket_request_msg req;
    req.mtype = 1;
    req.pid = getpid();
    
    /* Dla pierwszego rejsu, wiek generujemy zawsze z przedziału 15–69.
       Dla powracającego pasażera (is_second==1) również stosujemy ten przedział. */
    req.age = (rand() % (69 - 15 + 1)) + 15;
    
    /* Dla pierwszego rejsu zawsze ustawiamy second_trip = 0.
       Pasażer decyduje na powrót dopiero po zakończeniu rejsu.
       Dla powracającego pasażera (uruchomionego z "second") ustawiamy second_trip = 1. */
    req.second_trip = is_second ? 1 : 0;
    
    /* Tylko dla pierwszego rejsu (is_second==0) dajemy szansę na opiekuna.
       Dla powracającego pasażera zakładamy, że wraca sam. */
    if (!is_second) {
        req.with_guardian = (rand() % 2) ? 1 : 0;
    } else {
        req.with_guardian = 0;
    }
    
    printf("[Pasażer] Wiek: %d, pid: %d. Pojawił się.%s\n", req.age, req.pid, (is_second ? " (DRUGI REJS)" : ""));
    
    if (!is_second && req.with_guardian == 1) {
        int dependent_age;
        /* Losujemy wiek opiekuna: albo dziecko (0–14) albo senior (70–100) */
        if (rand() % 2 == 0)
            dependent_age = rand() % 15;
        else
            dependent_age = (rand() % (100 - 70 + 1)) + 70;
        pid_t child_pid = fork();
        if(child_pid < 0) {
            perror("fork (opiekun)");
        } else if(child_pid == 0) {
            printf("[Opiekun] Dependent: pid: %d, wiek: %d, towarzyszy pasażerowi: %d.\n", getpid(), dependent_age, getppid());
            fflush(stdout);
            exit(0);
        } else {
            printf("[Pasażer] Jestem z opiekunem: pid potomnego: %d, wiek potomnego: %d.\n", child_pid, dependent_age);
        }
    }
    
    if(msgsnd(msq_ticket, &req, sizeof(req) - sizeof(long), 0) == -1) {
        perror("msgsnd (ticket request)");
        exit(EXIT_FAILURE);
    }
    
    ticket_response_msg resp;
    if(msgrcv(msq_ticket, &resp, sizeof(resp) - sizeof(long), req.pid, 0) == -1) {
        perror("msgrcv (ticket response)");
        exit(EXIT_FAILURE);
    }
    printf("[Pasażer] Odebrał bilet: Statek %d, Cena: %d.\n", resp.boat_assigned, resp.price);
    
    int msq_boarding = msgget(MSG_KEY_BOARDING, 0666);
    if(msq_boarding == -1) {
        perror("msgget (boarding)");
        exit(EXIT_FAILURE);
    }
    
    boarding_request_msg board_req;
    /* Jeśli pasażer jest powracający (second_trip==1) ustawiamy mtype = 2, czyli omijanie kolejki */
    board_req.mtype = (req.second_trip == 1) ? 2 : 1;
    board_req.pid = getpid();
    board_req.boat = resp.boat_assigned;
    board_req.with_guardian = req.with_guardian;
    
    sleep(1);
    
    if(msgsnd(msq_boarding, &board_req, sizeof(board_req) - sizeof(long), 0) == -1) {
        perror("msgsnd (boarding request)");
        exit(EXIT_FAILURE);
    }
    
    return 0;
}
