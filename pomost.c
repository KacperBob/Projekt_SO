#include <stdio.h>          // Standardowa biblioteka wejścia/wyjścia
#include <stdlib.h>         // Funkcje ogólne, m.in. exit, atoi
#include <sys/msg.h>        // Funkcje związane z kolejkami komunikatów IPC (msgget, msgrcv, msgsnd)
#include <sys/shm.h>        // Funkcje związane z pamięcią dzieloną (shmget, shmat, shmdt)
#include <unistd.h>         // Funkcje systemowe, np. usleep, sleep
#include <errno.h>          // Definicje zmiennej errno oraz stałych błędów
#include <semaphore.h>      // Funkcje semaforów POSIX (sem_wait, sem_post)
#include "common.h"         // Definicje stałych, struktur komunikatów oraz pamięci dzielonej

// Funkcja główna programu – odpowiada za obsługę wejścia pasażerów na statki.
int main() {
    // Uzyskujemy dostęp do kolejki komunikatów o kluczu MSG_KEY_BOARDING,
    // która służy do odbierania żądań wejścia pasażerów na pokład (boarding request).
    int msq_boarding = msgget(MSG_KEY_BOARDING, 0666 | IPC_CREAT);
    if (msq_boarding == -1) {
        perror("msgget (boarding) in pomost");
        exit(EXIT_FAILURE);
    }
    
    // Uzyskujemy dostęp do segmentu pamięci dzielonej o kluczu SHM_KEY_BOATS,
    // który zawiera strukturę boats_shm_t przechowującą stan statków.
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
    
    // Zmienna do przechowywania odebranego komunikatu żądania wejścia na pokład
    boarding_request_msg board_req;
    
    // Główna pętla, w której pomost cyklicznie sprawdza czy pojawiło się żądanie wejścia na statek
    while (1) {
        // Odbieramy komunikat z kolejki MSG_KEY_BOARDING w trybie nieblokującym (IPC_NOWAIT)
        int ret = msgrcv(msq_boarding, &board_req, sizeof(board_req) - sizeof(long), 0, IPC_NOWAIT);
        if (ret == -1) {
            // Jeżeli brak komunikatów (errno == ENOMSG), krótko usypiamy i kontynuujemy pętlę
            if (errno == ENOMSG) {
                usleep(100000);
                continue;
            } else {
                // W przypadku innego błędu wypisujemy komunikat i kontynuujemy pętlę
                perror("msgrcv (boarding request) in pomost");
                continue;
            }
        }
        
        // Blokujemy semafor, aby uzyskać wyłączny dostęp do stanu statków w pamięci dzielonej
        sem_wait(&boats->occupancy_sem);
        
        // Sprawdzamy, do którego statku ma wejść pasażer (board_req.boat zawiera numer statku)
        if (board_req.boat == 1) {
            // Dla statku nr 1:
            // Jeśli statek jest w rejsie lub boarding został zamknięty, pasażer nie może wejść
            if (boats->boat1_in_trip || !boats->boat1_boarding_open) {
                printf("[Pomost] Statek 1 jest zamknięty lub w rejsie, pasażer: %d nie może wejść.\n", board_req.pid);
                sem_post(&boats->occupancy_sem); // Odblokowujemy semafor przed kontynuacją
                continue;
            }
            // Jeśli są jeszcze wolne miejsca na statku nr 1, dodajemy pasażera
            if (boats->occupancy_boat1 < BOAT1_CAPACITY) {
                // Zapisujemy PID pasażera w tablicy boat1_pids
                boats->boat1_pids[boats->occupancy_boat1] = board_req.pid;
                // Zwiększamy licznik zajętych miejsc na statku nr 1
                boats->occupancy_boat1++;
                int remaining = BOAT1_CAPACITY - boats->occupancy_boat1;
                printf("[Pomost] Pasażer: %d wszedł na statek 1. Pozostałych miejsc: %d\n", board_req.pid, remaining);
            } else {
                // Jeżeli statek nr 1 jest pełny, wypisujemy komunikat o braku wolnych miejsc
                printf("[Pomost] Pasażer: %d nie może wejść na statek 1 – pełny.\n", board_req.pid);
            }
        } else if (board_req.boat == 2) {
            // Dla statku nr 2:
            // Sprawdzamy, czy statek nr 2 jest aktualnie w rejsie lub czy boarding jest otwarty
            if (boats->boat2_in_trip || !boats->boat2_boarding_open) {
                printf("[Pomost] Statek 2 jest zamknięty lub w rejsie, pasażer: %d nie może wejść.\n", board_req.pid);
                sem_post(&boats->occupancy_sem);
                continue;
            }
            // Jeśli pasażer pojawia się z opiekunem, trzeba przydzielić miejsce dla obu
            if (board_req.with_guardian == 1) {
                if (boats->occupancy_boat2 <= BOAT2_CAPACITY - 2) {
                    // Zapisujemy PID pasażera – przyjmujemy, że opiekun zajmuje dodatkowe miejsce
                    boats->boat2_pids[boats->occupancy_boat2] = board_req.pid;
                    // Zwiększamy occupancy o 2, gdyż rezerwujemy miejsce dla pasażera i opiekuna
                    boats->occupancy_boat2 += 2;
                    int remaining = BOAT2_CAPACITY - boats->occupancy_boat2;
                    printf("[Pomost] Pasażer: %d (z opiekunem) wszedł na statek 2. Pozostałych miejsc: %d\n", board_req.pid, remaining);
                } else {
                    // Jeśli nie ma wystarczającej liczby miejsc dla pary, komunikat o braku miejsc
                    printf("[Pomost] Pasażer: %d (z opiekunem) nie może wejść na statek 2 – za mało miejsc.\n", board_req.pid);
                }
            } else {
                // Dla pasażera bez opiekuna
                if (boats->occupancy_boat2 < BOAT2_CAPACITY) {
                    boats->boat2_pids[boats->occupancy_boat2] = board_req.pid;
                    boats->occupancy_boat2++;
                    int remaining = BOAT2_CAPACITY - boats->occupancy_boat2;
                    printf("[Pomost] Pasażer: %d wszedł na statek 2. Pozostałych miejsc: %d\n", board_req.pid, remaining);
                } else {
                    // Jeżeli statek nr 2 jest pełny, komunikat o braku miejsc
                    printf("[Pomost] Pasażer: %d nie może wejść na statek 2 – pełny.\n", board_req.pid);
                }
            }
        }
        // Odblokowujemy semafor – zakończenie krytycznej sekcji dostępu do pamięci dzielonej
        sem_post(&boats->occupancy_sem);
        
        // Krótkie usypianie, aby zmniejszyć obciążenie CPU
        usleep(100000);
    }
    
    // Odłączamy segment pamięci dzielonej przed zakończeniem (teoretycznie, gdyby pętla była przerywana)
    shmdt(boats);
    return 0;
}
