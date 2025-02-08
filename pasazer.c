#include <stdio.h>      // Standardowa biblioteka wejścia/wyjścia (printf, fprintf, perror, fflush)
#include <stdlib.h>     // Funkcje ogólnego przeznaczenia (exit, rand, srand)
#include <string.h>     // Operacje na ciągach znaków (strcmp)
#include <sys/msg.h>    // Funkcje do obsługi kolejek komunikatów (msgget, msgsnd, msgrcv)
#include <unistd.h>     // Funkcje systemowe (fork, getpid, sleep, execl)
#include <sys/wait.h>   // Funkcje do oczekiwania na zakończenie procesów (wait, waitpid)
#include <time.h>       // Obsługa czasu (time)
#include "common.h"     // Wspólne definicje, struktury i stałe dla wszystkich procesów

// Główna funkcja programu "pasażer"
int main(int argc, char *argv[]) {
    int is_second = 0;  // Flaga oznaczająca, czy jest to drugi rejs (0 = pierwszy rejs, 1 = drugi rejs)
    
    /* Sprawdzenie liczby i poprawności argumentów wywołania.
       Jeśli program wywołany bez argumentów, to przyjmujemy, że jest to pierwszy rejs.
       Jeśli jako argument podano "second", to ustawiamy flagę is_second = 1.
       W przeciwnym razie wyświetlamy instrukcję użycia i kończymy program. */
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
    
    // Inicjalizacja generatora liczb losowych przy użyciu aktualnego czasu i identyfikatora procesu
    srand(time(NULL) ^ getpid());
    
    /* Otwarcie kolejki komunikatów dla żądań biletowych.
       Klucz MSG_KEY_TICKET jest zdefiniowany w common.h.
       Kolejka musi już istnieć (utworzona przez inny proces, np. kasjera). */
    int msq_ticket = msgget(MSG_KEY_TICKET, 0666);
    if(msq_ticket == -1) {
        perror("msgget (ticket)");
        exit(EXIT_FAILURE);
    }
    
    /* Przygotowanie żądania biletu do wysłania do kasjera.
       Struktura ticket_request_msg zawiera:
         - mtype: typ komunikatu (ustawiamy na 1)
         - pid: identyfikator procesu (pasażera)
         - age: losowy wiek z przedziału 15–69
         - second_trip: informacja, czy to drugi rejs (0 lub 1)
         - with_guardian: informacja, czy pasażer pojawia się z opiekunem (0 lub 1) */
    ticket_request_msg req;
    req.mtype = 1;
    req.pid = getpid();
    req.age = (rand() % (69 - 15 + 1)) + 15;  // Losujemy wiek w przedziale [15, 69]
    req.second_trip = is_second ? 1 : 0;
    if(!is_second)
        req.with_guardian = (rand() % 2) ? 1 : 0;  // Losujemy 50% szans na pojawienie się z opiekunem, jeśli to pierwszy rejs
    else
        req.with_guardian = 0;
    
    // Informacja na standardowe wyjście, że pasażer się pojawił
    printf("[Pasażer] Wiek: %d, pid: %d. Pojawił się.%s\n",
           req.age, req.pid, (is_second ? " (DRUGI REJS)" : ""));
    
    /* Jeśli pasażer pojawia się z opiekunem i to jest pierwszy rejs,
       tworzymy dodatkowy proces – opiekuna.
       Proces potomny (opiekun) wypisuje swoje dane i kończy działanie.
       Używamy signal(SIGCHLD, SIG_IGN) aby nie czekać na zakończenie procesu opiekuna. */
    if(!is_second && req.with_guardian == 1) {
        signal(SIGCHLD, SIG_IGN);
        pid_t child_pid = fork();
        if(child_pid < 0) {
            perror("fork (opiekun)");
        } else if(child_pid == 0) {
            int dependent_age;
            // Losujemy wiek opiekuna – albo bardzo młody, albo starszy
            if(rand() % 2 == 0)
                dependent_age = rand() % 15;
            else
                dependent_age = (rand() % (100 - 70 + 1)) + 70;
            printf("[Opiekun] Dependent: pid: %d, wiek: %d, towarzyszy pasażerowi: %d.\n",
                   getpid(), dependent_age, getppid());
            fflush(stdout);
            exit(0);
        } else {
            // Wypisujemy informację o procesie opiekuna
            printf("[Pasażer] Jestem z opiekunem: pid potomnego: %d.\n", child_pid);
        }
    }
    
    // Wysyłamy żądanie biletu do kasjera poprzez kolejkę komunikatów
    if(msgsnd(msq_ticket, &req, sizeof(req) - sizeof(long), 0) == -1) {
        perror("msgsnd (ticket request)");
        exit(EXIT_FAILURE);
    }
    
    /* Oczekiwanie na odpowiedź z biletem od kasjera.
       Kasjer odpowiada komunikatem typu ticket_response_msg, którego mtype jest ustawiony na pid pasażera. */
    ticket_response_msg resp;
    if(msgrcv(msq_ticket, &resp, sizeof(resp) - sizeof(long), req.pid, 0) == -1) {
        perror("msgrcv (ticket response)");
        exit(EXIT_FAILURE);
    }
    printf("[Pasażer] Odebrał bilet: Statek %d, Cena: %d.\n", resp.boat_assigned, resp.price);
    
    /* Otwarcie kolejki komunikatów dla żądania wejścia na pokład (boarding).
       Klucz MSG_KEY_BOARDING jest zdefiniowany w common.h. */
    int msq_boarding = msgget(MSG_KEY_BOARDING, 0666);
    if(msq_boarding == -1) {
        perror("msgget (boarding)");
        exit(EXIT_FAILURE);
    }
    
    /* Przygotowanie komunikatu żądania wejścia na pokład.
       Struktura boarding_request_msg zawiera:
         - mtype: typ komunikatu – ustawiamy 1 dla zwykłych pasażerów,
                   2 dla powracających (drugi rejs), którzy omijają kolejkę
         - pid: identyfikator pasażera
         - boat: numer statku przydzielony przez kasjera
         - with_guardian: informacja, czy pasażer ma opiekuna */
    boarding_request_msg board_req;
    board_req.mtype = (req.second_trip == 1) ? 2 : 1;
    board_req.pid = getpid();
    board_req.boat = resp.boat_assigned;
    board_req.with_guardian = req.with_guardian;
    
    // Krótka przerwa (1 sekunda) przed wysłaniem żądania wejścia na pokład
    sleep(1);
    
    // Wysyłamy komunikat żądania wejścia na pokład do pomostu
    if(msgsnd(msq_boarding, &board_req, sizeof(board_req) - sizeof(long), 0) == -1) {
        perror("msgsnd (boarding request)");
        exit(EXIT_FAILURE);
    }
    
    /* Jeśli to pierwszy rejs (is_second == 0):
       Oczekujemy na komunikat potwierdzający zakończenie rejsu od statku.
       Następnie losujemy szansę na drugi rejs (ustawioną na 25%).
       Jeśli pasażer zdecyduje się na drugi rejs, uruchamiamy ten sam program z argumentem "second".
       W przeciwnym przypadku kończymy działanie.
       (W tej wersji komunikaty informujące o rejsie zostały usunięte.) */
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
        
        // Losujemy liczbę z zakresu 0–99; jeśli jest mniejsza niż 25 (25% szans), pasażer decyduje się na drugi rejs.
        int r = rand() % 100;
        if(r < 25) {
            // Przechodzimy na drugi rejs – uruchamiamy ten sam program z argumentem "second"
            execl("./pasazer", "pasazer", "second", NULL);
            perror("execl (pasazer second)");
            exit(EXIT_FAILURE);
        }
        // Jeśli pasażer nie decyduje się na drugi rejs, kończymy program
        exit(EXIT_SUCCESS);
    } else {
        // Jeśli to drugi rejs, czekamy przez czas rejsu (T1 lub T2) plus dodatkowe 5 sekund
        int trip_time = (resp.boat_assigned == 1) ? T1 : T2;
        sleep(trip_time + 5);
        printf("[Pasażer] DRUGI rejs zakończony, opuszczam łódź.\n");
    }
    
    return 0;
}
