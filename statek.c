#include <stdio.h>
#include <stdlib.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <semaphore.h>
#include "common.h"

// Flaga informująca o przerwaniu rejsu (np. sygnał od policji)
volatile sig_atomic_t cancel_trip = 0;

// Obsługa sygnału – ustawienie flagi cancel_trip na 1
void sig_handler(int signo) { 
    cancel_trip = 1; 
}

int main(int argc, char *argv[]) {
    // Sprawdzenie, czy podano numer statku jako argument
    if(argc != 2) {
        fprintf(stderr, "Usage: %s <boat_number>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    int boat_number = atoi(argv[1]);
    // Ustalamy czas rejsu na podstawie numeru statku
    int trip_duration = (boat_number == 1) ? T1 : T2;
    
    // Rejestracja odpowiednich sygnałów: dla statku 1 obsługa SIGUSR1, dla statku 2 – SIGUSR2
    if(boat_number == 1)
        signal(SIGUSR1, sig_handler);
    else if(boat_number == 2)
        signal(SIGUSR2, sig_handler);
    
    // Otwarcie kolejki komunikatów dla komunikatów "depart"
    int msq_depart = msgget(MSG_KEY_DEPART, 0666 | IPC_CREAT);
    if(msq_depart == -1) {
        perror("msgget (depart) in statek");
        exit(EXIT_FAILURE);
    }
    
    // Dołączenie do segmentu pamięci dzielonej, w której przechowywany jest stan statków
    int shmid = shmget(SHM_KEY_BOATS, sizeof(boats_shm_t), 0666 | IPC_CREAT);
    if(shmid == -1) {
        perror("shmget (boats) in statek");
        exit(EXIT_FAILURE);
    }
    boats_shm_t *boats = (boats_shm_t *) shmat(shmid, NULL, 0);
    if(boats == (void *) -1) {
        perror("shmat (boats) in statek");
        exit(EXIT_FAILURE);
    }
    
    srand(time(NULL) ^ getpid());
    
    /* Na początek statek symuluje powrót z rejsu – opróżnia swój pokład,
       ustawia flagi otwarcia dla wejścia pasażerów oraz informuje o dostępnych miejscach. */
    sem_wait(&boats->occupancy_sem);
    if(boat_number == 1) {
        boats->occupancy_boat1 = 0;
        boats->boat1_boarding_open = 1;  // umożliwienie wejścia pasażerów
        boats->boat1_in_trip = 0;
        boats->boat1_start_time = 0;
    } else {
        boats->occupancy_boat2 = 0;
        boats->boat2_boarding_open = 1;
        boats->boat2_in_trip = 0;
        boats->boat2_start_time = 0;
    }
    sem_post(&boats->occupancy_sem);
    printf("[Statek] Statek nr %d właśnie wrócił z rejsu. Wszystkie miejsca są teraz wolne (%d miejsc dostępnych).\n", 
           boat_number, (boat_number == 1) ? BOAT1_CAPACITY : BOAT2_CAPACITY);
    
    // Główna pętla – statek oczekuje na komunikat depart i decyduje, czy odpłynąć
    while(1) {
        depart_msg dmsg;
        // Blokujące oczekiwanie na komunikat depart skierowany do tego statku (mtype = boat_number)
        if(msgrcv(msq_depart, &dmsg, sizeof(dmsg) - sizeof(long), boat_number, 0) == -1) {
            perror("msgrcv (depart) in statek");
            continue;
        }
        
        // Obsługa sytuacji, gdy rejs jest przerywany (np. sygnał od policji)
        if(cancel_trip) {
            printf("[Statek] Odebrano sygnał od Policji – statek %d nie wypływa, pasażerowie opuszczają łódź.\n", boat_number);
            sem_wait(&boats->occupancy_sem);
            if(boat_number == 1) {
                boats->occupancy_boat1 = 0;
                boats->boat1_boarding_open = 1;
                boats->boat1_in_trip = 0;
                boats->boat1_start_time = 0;
            } else {
                boats->occupancy_boat2 = 0;
                boats->boat2_boarding_open = 1;
                boats->boat2_in_trip = 0;
                boats->boat2_start_time = 0;
            }
            sem_post(&boats->occupancy_sem);
            cancel_trip = 0;
            continue;
        }
        
        // Sprawdzamy, czy na pokładzie są pasażerowie
        sem_wait(&boats->occupancy_sem);
        int occupancy = (boat_number == 1) ? boats->occupancy_boat1 : boats->occupancy_boat2;
        // Jeśli brak pasażerów, wypisujemy komunikat i wracamy do oczekiwania
        if (occupancy == 0) {
            sem_post(&boats->occupancy_sem);
            printf("[Statek] Brak pasażerów na pokładzie statku nr %d. Oczekuję na pasażerów...\n", boat_number);
            sleep(1);  // krótka przerwa przed kolejnym sprawdzeniem
            continue;
        }
        // Gdy są pasażerowie, ustawiamy flagi, że statek jest w rejsie
        if(boat_number == 1) {
            boats->boat1_in_trip = 1;
            boats->boat1_boarding_open = 0;
        } else {
            boats->boat2_in_trip = 1;
            boats->boat2_boarding_open = 0;
        }
        sem_post(&boats->occupancy_sem);
        
        // Informujemy, że statek odpływa z określoną liczbą pasażerów
        printf("[Statek] Odpływa statek nr %d z %d pasażerami.\n", boat_number, occupancy);
        sleep(trip_duration);  // symulacja trwania rejsu
        
        /* Po zakończeniu rejsu wysyłamy komunikaty "trip complete" do każdego pasażera,
           a następnie opróżniamy statek i przywracamy możliwość wejścia nowych pasażerów. */
        sem_wait(&boats->occupancy_sem);
        int msq_trip = msgget(MSG_KEY_TRIP, 0666 | IPC_CREAT);
        if(msq_trip == -1) {
            perror("msgget (trip complete) in statek");
        } else {
            if(boat_number == 1) {
                for (int i = 0; i < boats->occupancy_boat1; i++) {
                    trip_complete_msg tmsg;
                    tmsg.mtype = boats->boat1_pids[i]; // identyfikator pasażera jako mtype
                    if(msgsnd(msq_trip, &tmsg, sizeof(tmsg) - sizeof(long), 0) == -1) {
                        perror("msgsnd (trip complete) in statek");
                    }
                }
                boats->occupancy_boat1 = 0;
                boats->boat1_boarding_open = 1;
                boats->boat1_in_trip = 0;
                boats->boat1_start_time = 0;
            } else {
                for (int i = 0; i < boats->occupancy_boat2; i++) {
                    trip_complete_msg tmsg;
                    tmsg.mtype = boats->boat2_pids[i];
                    if(msgsnd(msq_trip, &tmsg, sizeof(tmsg) - sizeof(long), 0) == -1) {
                        perror("msgsnd (trip complete) in statek");
                    }
                }
                boats->occupancy_boat2 = 0;
                boats->boat2_boarding_open = 1;
                boats->boat2_in_trip = 0;
                boats->boat2_start_time = 0;
            }
        }
        sem_post(&boats->occupancy_sem);
        
        // Informujemy o powrocie statku i możliwościach wejścia pasażerów
        printf("[Statek] Statek nr %d właśnie wrócił z rejsu. Wszystkie miejsca są teraz wolne (%d miejsc dostępnych).\n",
               boat_number, (boat_number == 1) ? BOAT1_CAPACITY : BOAT2_CAPACITY);
    }
    
    shmdt(boats);
    return 0;
}
