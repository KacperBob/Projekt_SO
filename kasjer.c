#include <stdio.h>        // Standardowe wejście/wyjście
#include <stdlib.h>       // Funkcje ogólne (exit, itp.)
#include <sys/msg.h>      // Obsługa kolejek komunikatów IPC
#include <sys/shm.h>      // Obsługa pamięci dzielonej IPC
#include <unistd.h>       // Funkcje systemowe, np. sleep(), getpid()
#include <semaphore.h>    // Obsługa semaforów POSIX
#include "common.h"       // Wspólne definicje i struktury używane przez wszystkie moduły

int main() {
    // Uzyskujemy identyfikator kolejki komunikatów dla żądań biletów.
    int msq_ticket = msgget(MSG_KEY_TICKET, 0666 | IPC_CREAT);
    if(msq_ticket == -1) {
        perror("msgget (ticket) w kasjer"); // Wypisujemy błąd, jeśli nie udało się uzyskać kolejki
        exit(EXIT_FAILURE);
    }
    
    // Uzyskujemy identyfikator segmentu pamięci dzielonej przeznaczonej na strukturę statków.
    int shmid = shmget(SHM_KEY_BOATS, sizeof(boats_shm_t), 0666 | IPC_CREAT);
    if(shmid == -1) {
        perror("shmget (boats) w kasjer");  // Błąd przy tworzeniu lub otwieraniu segmentu pamięci dzielonej
        exit(EXIT_FAILURE);
    }
    // Przypisujemy wskaźnik do pamięci dzielonej z rzutowaniem na typ boats_shm_t.
    boats_shm_t *boats = (boats_shm_t *) shmat(shmid, NULL, 0);
    if(boats == (void *) -1) {
        perror("shmat (boats) w kasjer");  // Błąd przy mapowaniu segmentu pamięci dzielonej
        exit(EXIT_FAILURE);
    }
    
    // Inicjalizacja pamięci dzielonej (jeśli jeszcze nie została zainicjalizowana).
    if(boats->initialized != 1) {
        boats->occupancy_boat1 = 0;        // Ustawienie początkowej liczby pasażerów na statku 1
        boats->occupancy_boat2 = 0;        // Ustawienie początkowej liczby pasażerów na statku 2
        boats->boat1_boarding_open = 1;    // Ustawienie, że wejście na statek 1 jest otwarte
        boats->boat2_boarding_open = 1;    // Ustawienie, że wejście na statek 2 jest otwarte
        boats->boat1_in_trip = 0;          // Statek 1 nie jest aktualnie w rejsie
        boats->boat2_in_trip = 0;          // Statek 2 nie jest aktualnie w rejsie
        boats->boat1_start_time = 0;       // Brak rozpoczętego czasu rejsu dla statku 1
        boats->boat2_start_time = 0;       // Brak rozpoczętego czasu rejsu dla statku 2
        // Inicjalizacja semafora do synchronizacji dostępu do pól occupancy
        if(sem_init(&boats->occupancy_sem, 1, 1) == -1) {
            perror("sem_init w kasjer"); // Wypisujemy błąd, jeśli nie udało się zainicjalizować semafora
            exit(EXIT_FAILURE);
        }
        boats->initialized = 1;          // Ustawienie flagi inicjalizacji
    }
    
    // Główna pętla procesu kasjera – ciągłe przetwarzanie żądań biletu od pasażerów.
    while(1) {
        ticket_request_msg req;
        // Odbieramy żądanie biletu od kolejki komunikatów
        if(msgrcv(msq_ticket, &req, sizeof(req) - sizeof(long), 1, 0) == -1) {
            perror("msgrcv (ticket request) w kasjer"); // Błąd odbioru wiadomości
            continue; // W przypadku błędu kontynuujemy pętlę, aby próbować ponownie
        }
        
        int boat_assigned, price;
        // Decyzja o przydziale statku:
        // Jeśli pasażer jest z opiekunem, przydzielamy statek nr 2 (ograniczenia regulaminowe)
        if(req.with_guardian == 1) {
            boat_assigned = 2;
            price = 10;  // Ustalona cena biletu dla pasażera z opiekunem
        } else {
            boat_assigned = 1;
            price = 10;  // Cena biletu dla pasażera bez opiekuna (tutaj również 10)
        }
        // Jeśli pasażer decyduje się na drugi rejs, cena biletu jest obniżona o 50%
        if(req.second_trip == 1)
            price /= 2;
        
        // Wyświetlamy informację o obsłużeniu pasażera – numer PID, informację o opiekunie, wiek, zapłaconą cenę, przydzielony statek oraz informację czy to drugi rejs.
        printf("[Kasjer] Obsłużył Pasażera: %d%s, Wiek: %d, Zapłacił: %d, wysłał go na statek nr %d (DRUGI REJS: %s).\n",
               req.pid,
               (req.with_guardian ? " (z opiekunem)" : ""),
               req.age,
               price,
               boat_assigned,
               (req.second_trip ? "TAK" : "NIE"));
        
        // Przygotowujemy odpowiedź biletu, którą prześlemy z powrotem do pasażera.
        ticket_response_msg resp;
        resp.mtype = req.pid;      // Typ komunikatu ustawiamy na PID pasażera, aby wiadomość trafiła do właściwego odbiorcy.
        resp.boat_assigned = boat_assigned;
        resp.price = price;
        
        // Wysyłamy odpowiedź biletu do kolejki komunikatów.
        if(msgsnd(msq_ticket, &resp, sizeof(resp) - sizeof(long), 0) == -1) {
            perror("msgsnd (ticket response) w kasjer");  // Błąd wysyłki komunikatu
        }
    }
    
    // Odłączamy pamięć dzieloną przed zakończeniem (chociaż ten punkt kodu nigdy nie zostanie osiągnięty w pętli while(1)).
    shmdt(boats);
    return 0;
}
