#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/msg.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>
#include "common.h"

/* Klucz kolejki komunikatów potwierdzających zakończenie rejsu */
#define MSG_KEY_TRIP 3456

/* Struktura komunikatu potwierdzającego zakończenie rejsu */
typedef struct {
    long mtype;  // ustawiony na PID pasażera, który ma zakończyć swój proces
} trip_complete_msg;

int main(int argc, char *argv[]) {
    int is_second = 0;
    if (argc >= 2 && strcmp(argv[1], "second") == 0) {
        is_second = 1;
    }
    
    srand(time(NULL) ^ getpid());
    
    /* Pobranie kolejki biletowej */
    int msq_ticket = msgget(MSG_KEY_TICKET, 0666);
    if (msq_ticket == -1) {
        perror("msgget (ticket)");
        exit(EXIT_FAILURE);
    }
    
    ticket_request_msg req;
    req.mtype = 1;
    req.pid = getpid();
    
    /* Generujemy wiek pasażera z przedziału 15–69 zarówno dla pierwszego rejsu, jak i powrotnego */
    req.age = (rand() % (69 - 15 + 1)) + 15;
    
    /* Dla pierwszego rejsu ustawiamy second_trip = 0.
       Dla powracającego pasażera (uruchomionego z argumentem "second") ustawiamy second_trip = 1. */
    req.second_trip = is_second ? 1 : 0;
    
    /* Tylko dla pierwszego rejsu (is_second == 0) dajemy szansę (50%) na pojawienie się opiekuna */
    if (!is_second) {
        req.with_guardian = (rand() % 2) ? 1 : 0;
    } else {
        req.with_guardian = 0;
    }
    
    printf("[Pasażer] Wiek: %d, pid: %d. Pojawił się.%s\n",
           req.age, req.pid, (is_second ? " (DRUGI REJS)" : ""));
    
    /* Jeśli to pierwszy rejs i pasażer pojawia się z opiekunem, forkujemy proces opiekuna.
       Aby zapobiec powstawaniu zombie, ustawiamy SIGCHLD na SIG_IGN przed forkiem. */
    if (!is_second && req.with_guardian == 1) {
        signal(SIGCHLD, SIG_IGN);  // automatyczne reapowanie potomnych
        pid_t child_pid = fork();
        if (child_pid < 0) {
            perror("fork (opiekun)");
        } else if (child_pid == 0) {
            printf("[Opiekun] Dependent: pid: %d, wiek: %d, towarzyszy pasażerowi: %d.\n",
                   getpid(),
                   (rand() % 2 == 0 ? rand() % 15 : (rand() % (100 - 70 + 1)) + 70),
                   getppid());
            fflush(stdout);
            exit(0);
        } else {
            printf("[Pasażer] Jestem z opiekunem: pid potomnego: %d.\n", child_pid);
        }
    }
    
    /* Wysyłamy żądanie biletu */
    if (msgsnd(msq_ticket, &req, sizeof(req) - sizeof(long), 0) == -1) {
        perror("msgsnd (ticket request)");
        exit(EXIT_FAILURE);
    }
    
    ticket_response_msg resp;
    if (msgrcv(msq_ticket, &resp, sizeof(resp) - sizeof(long), req.pid, 0) == -1) {
        perror("msgrcv (ticket response)");
        exit(EXIT_FAILURE);
    }
    printf("[Pasażer] Odebrał bilet: Statek %d, Cena: %d.\n", resp.boat_assigned, resp.price);
    
    /* Wysyłamy żądanie wejścia na pokład */
    int msq_boarding = msgget(MSG_KEY_BOARDING, 0666);
    if (msq_boarding == -1) {
        perror("msgget (boarding)");
        exit(EXIT_FAILURE);
    }
    
    boarding_request_msg board_req;
    board_req.mtype = (req.second_trip == 1) ? 2 : 1;
    board_req.pid = getpid();
    board_req.boat = resp.boat_assigned;
    board_req.with_guardian = req.with_guardian;
    
    sleep(1);
    
    if (msgsnd(msq_boarding, &board_req, sizeof(board_req) - sizeof(long), 0) == -1) {
        perror("msgsnd (boarding request)");
        exit(EXIT_FAILURE);
    }
    
    /* Po wysłaniu boarding_request, pasażer oczekuje na komunikat zakończenia rejsu.
       Pobieramy kolejkę komunikatów potwierdzających zakończenie rejsu */
    int msq_trip = msgget(MSG_KEY_TRIP, 0666 | IPC_CREAT);
    if (msq_trip == -1) {
        perror("msgget (trip complete)");
        exit(EXIT_FAILURE);
    }
    
    trip_complete_msg trip_msg;
    if (msgrcv(msq_trip, &trip_msg, sizeof(trip_msg) - sizeof(long), req.pid, 0) == -1) {
        perror("msgrcv (trip complete)");
        exit(EXIT_FAILURE);
    }
    
    printf("[Pasażer] Rejs zakończony, opuszczam łódź.\n");
    
    return 0;
}
