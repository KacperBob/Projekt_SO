#ifndef COMMON_H
#define COMMON_H

#include <sys/types.h>
#include <semaphore.h>
#include <time.h>

/* Parametry symulacji */
#define BOAT1_CAPACITY 7    // Statek nr 1: 7 miejsc
#define BOAT2_CAPACITY 4    // Statek nr 2: 4 miejsca
#define POMOST_CAPACITY 2   // Pojemność molo
#define T1 10   // Czas rejsu statku 1 (sekundy)
#define T2 15   // Czas rejsu statku 2 (sekundy)
#define MAX_WAIT 10  // Maksymalny czas oczekiwania na załadunek (sekundy)

/* Klucze IPC */
#define MSG_KEY_TICKET 1234
#define MSG_KEY_BOARDING 5678
#define MSG_KEY_DEPART 9012
#define SHM_KEY_BOATS 2345
#define MSG_KEY_TRIP 3456  /* Nowy klucz kolejki dla komunikatów potwierdzających zakończenie rejsu */

/* Typ komunikatu potwierdzającego zakończenie rejsu */
typedef struct {
    long mtype;  /* Ustawiany na pid pasażera */
} trip_complete_msg;

/* Struktury komunikatów */

/* Żądanie biletu – pasażer → kasjer */
typedef struct {
    long mtype;           // Ustawiamy na 1
    pid_t pid;
    int age;
    int second_trip;      // 0 lub 1 – 1 = powracający (drugi rejs)
    int with_guardian;    // 0 lub 1 – 1, gdy pasażer pojawia się z opiekunem
} ticket_request_msg;

/* Odpowiedź biletu – kasjer → pasażer (mtype = pid pasażera) */
typedef struct {
    long mtype;
    int boat_assigned;    // 1 lub 2
    int price;
} ticket_response_msg;

/* Żądanie wejścia na pokład – pasażer → pomost */
typedef struct {
    long mtype;           // mtype = 1 dla zwykłych, 2 dla powracających (omijających kolejkę)
    pid_t pid;
    int boat;             // numer statku przydzielony przez kasjera
    int with_guardian;
} boarding_request_msg;

/* Komunikat odpłynięcia – sternik → statek (mtype = numer statku) */
typedef struct {
    long mtype;
} depart_msg;

/* Pamięć dzielona – stan statków */
typedef struct {
    int occupancy_boat1;
    int occupancy_boat2;
    int boat1_boarding_open;  // 1 = otwarty, 0 = zamknięty
    int boat2_boarding_open;  // 1 = otwarty, 0 = zamknięty
    int boat1_in_trip;        // 0 = nie w rejsie, 1 = w rejsie
    int boat2_in_trip;
    time_t boat1_start_time;
    time_t boat2_start_time;
    int initialized;          // 0 = nie, 1 = tak
    sem_t occupancy_sem;      // semafor do synchronizacji dostępu do occupancy
    pid_t boat1_pids[BOAT1_CAPACITY];  // identyfikatory pasażerów dla statku 1
    pid_t boat2_pids[BOAT2_CAPACITY];  // identyfikatory pasażerów dla statku 2
} boats_shm_t;

#endif
