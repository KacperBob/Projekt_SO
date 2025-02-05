#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/msg.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>
#include "common.h"

int main(int argc, char *argv[]) {
    int is_second = 0;
    if(argc == 1) {
        is_second = 0;
    } else if(argc == 2) {
        if(strcmp(argv[1], "second") == 0)
            is_second = 1;
        else {
            fprintf(stderr, "Usage: %s [second]\n", argv[0]);
            exit(EXIT_FAILURE);
        }
    } else {
        fprintf(stderr, "Usage: %s [second]\n", argv[0]);
        exit(EXIT_FAILURE);
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
    req.age = (rand() % (69 - 15 + 1)) + 15;
    req.second_trip = is_second ? 1 : 0;
    if(!is_second)
        req.with_guardian = (rand() % 2) ? 1 : 0;
    else
        req.with_guardian = 0;
    
    printf("[Pasażer] Wiek: %d, pid: %d. Pojawił się.%s\n",
           req.age, req.pid, (is_second ? " (DRUGI REJS)" : ""));
    
    if(!is_second && req.with_guardian == 1) {
        signal(SIGCHLD, SIG_IGN);
        pid_t child_pid = fork();
        if(child_pid < 0) {
            perror("fork (opiekun)");
        } else if(child_pid == 0) {
            int dependent_age;
            if(rand() % 2 == 0)
                dependent_age = rand() % 15;
            else
                dependent_age = (rand() % (100 - 70 + 1)) + 70;
            printf("[Opiekun] Dependent: pid: %d, wiek: %d, towarzyszy pasażerowi: %d.\n",
                   getpid(), dependent_age, getppid());
            fflush(stdout);
            exit(0);
        } else {
            printf("[Pasażer] Jestem z opiekunem: pid potomnego: %d.\n", child_pid);
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
    board_req.mtype = (req.second_trip == 1) ? 2 : 1;
    board_req.pid = getpid();
    board_req.boat = resp.boat_assigned;
    board_req.with_guardian = req.with_guardian;
    
    sleep(1);
    
    if(msgsnd(msq_boarding, &board_req, sizeof(board_req) - sizeof(long), 0) == -1) {
        perror("msgsnd (boarding request)");
        exit(EXIT_FAILURE);
    }
    
    if(!is_second) {
        int msq_trip = msgget(MSG_KEY_TRIP, 0666 | IPC_CREAT);
        if(msq_trip == -1) {
            perror("msgget (trip complete)");
            exit(EXIT_FAILURE);
        }
        trip_complete_msg trip_msg;
        if(msgrcv(msq_trip, &trip_msg, sizeof(trip_msg) - sizeof(long), req.pid, 0) == -1) {
            perror("msgrcv (trip complete)");
            exit(EXIT_FAILURE);
        }
        printf("[Pasażer] Rejs zakończony, opuszczam łódź.\n");
        
        /* Ustawiamy 100% szansy na drugi rejs – dla celów testowych */
        printf("[Pasażer] (Test) Zdecydowanie wybieram DRUGI REJS.\n");
        execl("./pasazer", "pasazer", "second", NULL);
        perror("execl (pasazer second)");
        exit(EXIT_FAILURE);
    } else {
        int trip_time = (resp.boat_assigned == 1) ? T1 : T2;
        sleep(trip_time + 5);
        printf("[Pasażer] DRUGI rejs zakończony, opuszczam łódź.\n");
    }
    
    return 0;
}
