#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include "common.h"

/*
 * Funkcja main:
 * 1. Sprawdza, czy zostały podane dwa argumenty wiersza poleceń – identyfikatory procesów (PID) statków (statek 1 i statek 2).
 * 2. Konwertuje podane argumenty (łańcuchy znaków) na liczby całkowite (PIDy).
 * 3. Inicjalizuje generator liczb losowych, wykorzystując bieżący czas oraz PID procesu, aby uzyskać lepszą losowość.
 * 4. W nieskończonej pętli losuje czas oczekiwania (od 60 do 120 sekund), po czym śpi przez wyliczony czas.
 * 5. Następnie losowo wybiera, do którego statku (lub obu) wysłać sygnał przerwania rejsu:
 *    - Jeśli wybór wynosi 0 – wysyłany jest sygnał SIGUSR1 tylko do statku 1.
 *    - Jeśli wybór wynosi 1 – wysyłany jest sygnał SIGUSR2 tylko do statku 2.
 *    - W przeciwnym razie – wysyłane są sygnały do obu statków.
 * 6. Po wysłaniu sygnałów, wymusza opróżnienie bufora wyjścia.
 * 7. Proces powtarza cały cykl w nieskończonej pętli.
 */

int main(int argc, char *argv[]) {
    // Sprawdzamy, czy użytkownik podał dokładnie dwa argumenty (PID statków)
    if(argc != 3) {
        fprintf(stderr, "Usage: %s <boat1_pid> <boat2_pid>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    // Konwersja argumentów na identyfikatory procesów (PID) statków
    pid_t boat1_pid = atoi(argv[1]);
    pid_t boat2_pid = atoi(argv[2]);
    
    // Inicjalizacja generatora liczb losowych – używamy bieżącego czasu XOR z PID procesu
    srand(time(NULL) ^ getpid());
    
    // Główna pętla działania procesu Policja
    while(1) {
        // Losujemy czas oczekiwania – wartość z przedziału 60-120 sekund
        // rand() % 61 daje liczbę od 0 do 60; dodając 60 otrzymujemy przedział 60-120.
        int wait_time = (rand() % 61) + 60;
        sleep(wait_time);
        
        // Losujemy wartość z przedziału 0–2
        // 0: wysyłamy sygnał tylko do statku 1,
        // 1: wysyłamy sygnał tylko do statku 2,
        // 2: wysyłamy sygnały do obu statków.
        int choice = rand() % 3;
        if(choice == 0) {
            // Wysyłamy sygnał SIGUSR1 do statku 1 (przerwanie rejsu)
            kill(boat1_pid, SIGUSR1);
            printf("[Policja] Wysłano sygnał do statku 1 (przerwanie rejsu).\n");
        } else if(choice == 1) {
            // Wysyłamy sygnał SIGUSR2 do statku 2 (przerwanie rejsu)
            kill(boat2_pid, SIGUSR2);
            printf("[Policja] Wysłano sygnał do statku 2 (przerwanie rejsu).\n");
        } else {
            // Wysyłamy sygnały do obu statków
            kill(boat1_pid, SIGUSR1);
            kill(boat2_pid, SIGUSR2);
            printf("[Policja] Wysłano sygnały do obu statków (przerwanie rejsu).\n");
        }
        
        // Wymuszamy natychmiastowe wypisanie komunikatów na standardowe wyjście
        fflush(stdout);
    }
    
    return 0;
}
