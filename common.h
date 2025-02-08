#ifndef COMMON_H
#define COMMON_H

#include <sys/types.h>
#include <semaphore.h>
#include <time.h>

/* Parametry symulacji */
// Maksymalna liczba miejsc na statkach
#define BOAT1_CAPACITY 7    // Statek nr 1: 7 miejsc
#define BOAT2_CAPACITY 4    // Statek nr 2: 4 miejsca
#define POMOST_CAPACITY 2   // Pojemność molo (maksymalna liczba pasażerów, którzy mogą jednocześnie korzystać z pomostu)

// Czas trwania rejsów dla poszczególnych statków (w sekundach)
#define T1 10   // Czas rejsu statku 1
#define T2 15   // Czas rejsu statku 2

// Maksymalny czas oczekiwania na załadunek (w sekundach)
#define MAX_WAIT 10  

// Czas oczekiwania na wejście pasażerów po powrocie statku z rejsu (czas boarding)
// (czyli okres, w którym statek ogłasza, że właśnie wrócił, a pokład jest dostępny)
#define BOARDING_TIME 5  

/* Klucze IPC */
// Klucz kolejki komunikatów do obsługi biletu
#define MSG_KEY_TICKET 1234
// Klucz kolejki komunikatów do obsługi wejścia na pokład
#define MSG_KEY_BOARDING 5678
// Klucz kolejki komunikatów do wysyłania sygnału odpłynięcia (depart)
#define MSG_KEY_DEPART 9012
// Klucz pamięci dzielonej przechowującej stan statków
#define SHM_KEY_BOATS 2345
// Klucz kolejki komunikatów do wysyłania komunikatów potwierdzających zakończenie rejsu
#define MSG_KEY_TRIP 3456

/* Typ komunikatu potwierdzającego zakończenie rejsu */
// Każdy komunikat ma typ ustawiony na PID pasażera, który ma otrzymać potwierdzenie.
typedef struct {
    long mtype;  /* Ustawiany na PID pasażera */
} trip_complete_msg;

/* Struktury komunikatów */

/* Żądanie biletu – przesyłane przez pasażera do kasjera */
typedef struct {
    long mtype;           // Ustawiamy na 1
    pid_t pid;            // PID pasażera
    int age;              // Wiek pasażera
    int second_trip;      // 0 lub 1 – 1 = powracający (drugi rejs)
    int with_guardian;    // 0 lub 1 – 1, gdy pasażer pojawia się z opiekunem
} ticket_request_msg;

/* Odpowiedź biletu – przesyłana przez kasjera do pasażera (mtype ustawione na PID pasażera) */
typedef struct {
    long mtype;
    int boat_assigned;    // Numer statku przydzielony pasażerowi (1 lub 2)
    int price;            // Cena biletu (może być obniżona dla drugiego rejsu)
} ticket_response_msg;

/* Żądanie wejścia na pokład – przesyłane przez pasażera do procesu pomostu */
typedef struct {
    long mtype;           // mtype = 1 dla zwykłych pasażerów, 2 dla powracających (omijających kolejkę)
    pid_t pid;            // PID pasażera
    int boat;             // Numer statku przydzielony przez kasjera
    int with_guardian;    // Informacja, czy pasażer jest z opiekunem
} boarding_request_msg;

/* Komunikat odpłynięcia – przesyłany przez proces sternika do statku
   (mtype = numer statku, czyli 1 lub 2) */
typedef struct {
    long mtype;
} depart_msg;

/* Struktura pamięci dzielonej – stan statków */
// Przechowuje aktualne dane dotyczące liczby pasażerów, statusu boardingu,
// czy statek jest w rejsie, czas rozpoczęcia rejsu oraz tablice identyfikatorów pasażerów.
typedef struct {
    int occupancy_boat1;         // Aktualna liczba pasażerów na statku 1
    int occupancy_boat2;         // Aktualna liczba pasażerów na statku 2
    int boat1_boarding_open;     // 1 = pokład statku 1 jest otwarty, 0 = zamknięty
    int boat2_boarding_open;     // 1 = pokład statku 2 jest otwarty, 0 = zamknięty
    int boat1_in_trip;           // 0 = statek 1 nie jest w rejsie, 1 = w rejsie
    int boat2_in_trip;           // Analogicznie dla statku 2
    time_t boat1_start_time;     // Czas rozpoczęcia rejsu statku 1
    time_t boat2_start_time;     // Czas rozpoczęcia rejsu statku 2
    int initialized;             // Flaga inicjalizacji (0 = nie, 1 = tak)
    sem_t occupancy_sem;         // Semafor do synchronizacji dostępu do pól occupancy
    pid_t boat1_pids[BOAT1_CAPACITY];  // Tablica PID-ów pasażerów na statku 1
    pid_t boat2_pids[BOAT2_CAPACITY];  // Tablica PID-ów pasażerów na statku 2
} boats_shm_t;

#endif
