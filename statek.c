#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <time.h>

#define FIFO_PATH1 "fifo_boat1"
#define FIFO_PATH2 "fifo_boat2"
#define SHM_KEY_TIME 5678
#define TIME_SIZE 20

#define MAX_SEATS_BOAT1 10
#define MAX_SEATS_BOAT2 10

int seats_boat1 = 0;
int seats_boat2 = 0;
char *shared_time;
int shmid_time;
time_t last_passenger_time_boat1;
time_t last_passenger_time_boat2;

void cleanup() {
    unlink(FIFO_PATH1);
    unlink(FIFO_PATH2);
    if (shared_time != NULL && shmdt(shared_time) == -1) {
        perror("Nie można odłączyć pamięci współdzielonej dla czasu");
    }
    printf("FIFO files and shared memory removed.\n");
    exit(0);
}

void handle_signal(int sig) {
    cleanup();
}

void process_fifo(int fd, const char *boat_name, int *current_seats, int max_seats, time_t *last_passenger_time) {
    char buffer[256];
    memset(buffer, 0, sizeof(buffer));
    ssize_t bytes_read = read(fd, buffer, sizeof(buffer) - 1);
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0'; // Null-terminate the string 
        char current_time[TIME_SIZE];
        strncpy(current_time, shared_time, TIME_SIZE);
        current_time[TIME_SIZE - 1] = '\0';

        if (*current_seats < max_seats) {
            (*current_seats)++;
            *last_passenger_time = time(NULL); // Aktualizacja czasu ostatniego pasażera
            printf("[%s] %s: Pasażer: %s zajął miejsce %d/%d.\n", current_time, boat_name, buffer, *current_seats, max_seats);
        } else {
            printf("[%s] %s: Wszystkie miejsca zajęte! Pasażer: %s odrzucony.\n", current_time, boat_name, buffer);
        }
    } else if (bytes_read == 0) {
        // Nie wyświetlaj komunikatu, gdy nie ma danych
    } else {
        perror("Błąd odczytu z FIFO");
    }
}

void check_and_start_trip(const char *boat_name, int *current_seats, int max_seats, time_t *last_passenger_time) {
    time_t current_time = time(NULL);
    double time_diff = difftime(current_time, *last_passenger_time);

    if (*current_seats == max_seats || time_diff >= 20) {
        printf("[%s] %s: Rozpoczynam rejs. Liczba pasażerów na statku: %d.\n", shared_time, boat_name, *current_seats);
        sleep(5); // Symulacja czasu trwania rejsu
        printf("[%s] %s: Rejs zakończony. Pasażerowie mogą schodzić.\n", shared_time, boat_name);
        *current_seats = 0;
        *last_passenger_time = time(NULL); // Reset czasu ostatniego pasażera
    }
}

int main() {
    // Tworzenie pamięci współdzielonej dla czasu
    shmid_time = shmget(SHM_KEY_TIME, TIME_SIZE, 0666);
    if (shmid_time == -1) {
        perror("Nie można uzyskać dostępu do pamięci współdzielonej dla czasu");
        exit(1);
    }

    shared_time = (char *)shmat(shmid_time, NULL, 0);
    if (shared_time == (char *)-1) {
        perror("Nie można przyłączyć pamięci współdzielonej dla czasu");
        exit(1);
    }

    // Tworzenie FIFO
    if (mkfifo(FIFO_PATH1, 0666) == -1 && errno != EEXIST) {
        perror("Nie udało się utworzyć FIFO dla boat1");
        exit(1);
    } else {
        printf("FIFO dla boat1 utworzone lub już istnieje.\n");
    }

    if (mkfifo(FIFO_PATH2, 0666) == -1 && errno != EEXIST) {
        perror("Nie udało się utworzyć FIFO dla boat2");
        unlink(FIFO_PATH1);
        exit(1);
    } else {
        printf("FIFO dla boat2 utworzone lub już istnieje.\n");
    }

    printf("FIFO files created successfully.\n");

    // Obsługa sygnałów
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    // Otwieranie FIFO w trybie blokującym
    int fd_boat1 = open(FIFO_PATH1, O_RDONLY);
    if (fd_boat1 == -1) {
        perror("Nie udało się otworzyć FIFO dla boat1");
        cleanup();
    } else {
        printf("FIFO boat1 otwarte do odczytu.\n");
    }

    int fd_boat2 = open(FIFO_PATH2, O_RDONLY);
    if (fd_boat2 == -1) {
        perror("Nie udało się otworzyć FIFO dla boat2");
        cleanup();
    } else {
        printf("FIFO boat2 otwarte do odczytu.\n");
    }

    printf("FIFO files opened for reading.\n");

    // Inicjalizacja czasu ostatniego pasażera
    last_passenger_time_boat1 = time(NULL);
    last_passenger_time_boat2 = time(NULL);

    while (1) {
        // Odczyt z FIFO dla boat1
        process_fifo(fd_boat1, "Statek 1", &seats_boat1, MAX_SEATS_BOAT1, &last_passenger_time_boat1);

        // Odczyt z FIFO dla boat2
        process_fifo(fd_boat2, "Statek 2", &seats_boat2, MAX_SEATS_BOAT2, &last_passenger_time_boat2);

        // Sprawdzenie warunków dla rejsów
        check_and_start_trip("Statek 1", &seats_boat1, MAX_SEATS_BOAT1, &last_passenger_time_boat1);
        check_and_start_trip("Statek 2", &seats_boat2, MAX_SEATS_BOAT2, &last_passenger_time_boat2);

        usleep(100000); // Czas oczekiwania na kolejne dane
    }

    close(fd_boat1);
    close(fd_boat2);
    cleanup();

    return 0;
}
