// statek.c
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

#define SHM_KEY_TIME     5678
#define SHM_KEY_CAPACITY 8765
#define TIME_SIZE        20

#define MAX_SEATS_BOAT1 4
#define MAX_SEATS_BOAT2 4
#define TRIP_DURATION   5  // Czas trwania rejsu w sekundach
#define TIME_LIMIT      20 // Limit czasu oczekiwania na pasażera w sekundach

#define FIFO_STATEK1     "fifo_statek1"
#define FIFO_STATEK2     "fifo_statek2"
#define FIFO_STERNIK1    "fifo_ster1"
#define FIFO_STERNIK2    "fifo_ster2"
#define FIFO_LOG         "fifo_log"

struct pomost_state {
    int passengers_on_bridge;
    int direction; // 1 – wchodzenie na statek, -1 – schodzenie ze statku, 0 – blokada
};

int shmid_time = -1;
char *shared_time = NULL;
int shmid_capacities = -1;
int *sharedCap = NULL;

int seats_boat1 = 0;
int seats_boat2 = 0;
time_t last_passenger_time_boat1;
time_t last_passenger_time_boat2;

volatile sig_atomic_t running = 1;

void cleanup(int signum){
    if(shared_time != NULL){
        shmdt(shared_time);
    }
    if(sharedCap != NULL){
        shmdt(sharedCap);
    }
    // Nie usuwamy pamięci współdzielonej, ponieważ zarządza nią `kasjer.c` i `statek.c`
    // unlink(FIFO_STATEK1);
    // unlink(FIFO_STATEK2);
    printf("\nStatek zamknięty.\n");
    exit(0);
}

void getTime(char *b){
    if(shared_time == NULL){
        // Użyj systemowego czasu jako fallback
        time_t now = time(NULL);
        struct tm *tm_info = localtime(&now);
        strftime(b, TIME_SIZE, "%H:%M:%S", tm_info);
        return;
    }
    snprintf(b, TIME_SIZE, "%s", shared_time);
}

void logStatek(const char *s){
    int f = open("statekraport.txt", O_WRONLY | O_CREAT | O_APPEND, 0666);
    if(f >= 0){
        write(f, s, strlen(s));
        close(f);
    }
    printf("%s", s);
    fflush(stdout);
}

void process_fifo(int fd, const char *boat_name, int *current_seats, int max_seats){
    char buffer[128];
    ssize_t bytes_read;
    while((bytes_read = read(fd, buffer, sizeof(buffer) - 1)) > 0){
        buffer[bytes_read] = '\0'; // Null-terminate
        char czas[TIME_SIZE];
        getTime(czas);

        // Sprawdzenie, czy jest to komunikat o przypisaniu pasażera
        int pid, boat_num, free_seats;
        if(sscanf(buffer, "%d:%d:%d", &pid, &boat_num, &free_seats) == 3){
            // Aktualizacja lokalnych zmiennych
            *current_seats = free_seats;

            // Generowanie własnego komunikatu
            char log_msg[256];
            snprintf(log_msg, sizeof(log_msg),
                "[%s] statek: pasazer pid: %d. wsiadl na statek %d. Liczba miejsc na statku: %d.\n",
                czas, pid, boat_num, free_seats);
            logStatek(log_msg);
        }
        else{
            // Sprawdzenie, czy jest to polecenie od sternika
            if(strncmp(buffer, "START", 5) == 0){
                // Rozpoczęcie rejsu, jeśli są pasażerowie
                if((*current_seats) > 0){
                    char log_msg[256];
                    snprintf(log_msg, sizeof(log_msg),
                        "[%s] %s: Rozpoczynam rejs z %d pasażerami.\n",
                        czas, boat_name, *current_seats);
                    logStatek(log_msg);

                    // Symulacja rejsu
                    sleep(TRIP_DURATION);

                    snprintf(log_msg, sizeof(log_msg),
                        "[%s] %s: Rejs zakończony. Pasażerowie mogą schodzić.\n",
                        czas, boat_name);
                    logStatek(log_msg);

                    // Reset liczby pasażerów
                    *current_seats = 0;
                    // Aktualizacja pamięci współdzielonej
                    if(strcmp(boat_name, "Statek 1") == 0){
                        sharedCap[0] = *current_seats;
                    }
                    else{
                        sharedCap[1] = *current_seats;
                    }
                }
                else{
                    char log_msg[256];
                    snprintf(log_msg, sizeof(log_msg),
                        "[%s] %s: Brak pasażerów do rejsu.\n",
                        czas, boat_name);
                    logStatek(log_msg);
                }
            }
            else{
                // Inne komunikaty, np. błędne formatowanie
                char log_msg[256];
                snprintf(log_msg, sizeof(log_msg),
                    "[%s] %s: Niepoprawny format komunikatu: %s\n",
                    czas, boat_name, buffer);
                logStatek(log_msg);
            }
        }
    }

    if(bytes_read == -1 && errno != EAGAIN && errno != EWOULDBLOCK){
        perror("statek: read");
    }
}

void check_and_start_trip(const char *boat_name, int *current_seats, int max_seats, time_t *last_passenger_time){
    time_t current_time = time(NULL);
    double time_diff = difftime(current_time, *last_passenger_time);

    if(*current_seats == max_seats || time_diff >= TIME_LIMIT){
        if(*current_seats > 0){
            char czas[TIME_SIZE];
            getTime(czas);
            char log_msg[512];
            snprintf(log_msg, sizeof(log_msg),
                "[%s] %s: Rozpoczynam rejs z %d pasażerami.\n",
                czas, boat_name, *current_seats);
            logStatek(log_msg);

            // Simulacja rejsu
            sleep(TRIP_DURATION);

            getTime(czas);
            snprintf(log_msg, sizeof(log_msg),
                "[%s] %s: Rejs zakończony. Pasażerowie mogą schodzić.\n",
                czas, boat_name);
            logStatek(log_msg);

            // Reset liczby pasażerów
            *current_seats = 0;
            // Aktualizacja pamięci współdzielonej
            if(strcmp(boat_name, "Statek 1") == 0){
                sharedCap[0] = *current_seats;
            }
            else{
                sharedCap[1] = *current_seats;
            }
        }
    }
}

int main(){
    signal(SIGINT, cleanup);
    signal(SIGTERM, cleanup);

    // Łączenie z pamięcią współdzieloną dla czasu
    shmid_time = shmget(SHM_KEY_TIME, TIME_SIZE, 0666);
    while(shmid_time == -1){
        perror("statek: shmget czas");
        sleep(1);
        shmid_time = shmget(SHM_KEY_TIME, TIME_SIZE, 0666);
    }
    shared_time = shmat(shmid_time, NULL, 0);
    if(shared_time == (char*)-1){
        perror("statek: shmat czas");
        cleanup(0);
    }

    // Łączenie z pamięcią współdzieloną dla pojemności statków
    shmid_capacities = shmget(SHM_KEY_CAPACITY, 2 * sizeof(int), 0666);
    while(shmid_capacities == -1){
        perror("statek: shmget capacity");
        sleep(1);
        shmid_capacities = shmget(SHM_KEY_CAPACITY, 2 * sizeof(int), 0666);
    }
    sharedCap = (int*)shmat(shmid_capacities, NULL, 0);
    if(sharedCap == (int*)-1){
        perror("statek: shmat capacity");
        cleanup(0);
    }

    // Inicjalizacja lokalnych zmiennych z pamięci współdzielonej
    seats_boat1 = sharedCap[0];
    seats_boat2 = sharedCap[1];
    time(&last_passenger_time_boat1);
    time(&last_passenger_time_boat2);

    // Tworzenie FIFO, jeśli nie istnieją
    if(mkfifo(FIFO_STATEK1, 0666) == -1 && errno != EEXIST){
        perror("statek: mkfifo_statek1");
        cleanup(0);
    }
    if(mkfifo(FIFO_STATEK2, 0666) == -1 && errno != EEXIST){
        perror("statek: mkfifo_statek2");
        cleanup(0);
    }
    if(mkfifo(FIFO_STERNIK1, 0666) == -1 && errno != EEXIST){
        perror("statek: mkfifo_ster1");
        cleanup(0);
    }
    if(mkfifo(FIFO_STERNIK2, 0666) == -1 && errno != EEXIST){
        perror("statek: mkfifo_ster2");
        cleanup(0);
    }

    printf("Statek uruchomiony. Czeka na pasażerów i polecenia sternika.\n");
    logStatek("Statek uruchomiony. Czeka na pasażerów i polecenia sternika.\n");

    // Otwieranie FIFO w trybie nieblokującym
    int fd_statek1 = open(FIFO_STATEK1, O_RDONLY | O_NONBLOCK);
    if(fd_statek1 == -1){
        perror("statek: open fifo_statek1");
        cleanup(0);
    }
    int fd_statek2 = open(FIFO_STATEK2, O_RDONLY | O_NONBLOCK);
    if(fd_statek2 == -1){
        perror("statek: open fifo_statek2");
        cleanup(0);
    }

    // Otwieranie FIFO sternika w trybie nieblokującym
    int fd_ster1 = open(FIFO_STERNIK1, O_RDONLY | O_NONBLOCK);
    if(fd_ster1 == -1){
        perror("statek: open fifo_ster1");
        cleanup(0);
    }
    int fd_ster2 = open(FIFO_STERNIK2, O_RDONLY | O_NONBLOCK);
    if(fd_ster2 == -1){
        perror("statek: open fifo_ster2");
        cleanup(0);
    }

    while(running){
        // Odczyt pasażerów z FIFO_STATEK1
        process_fifo(fd_statek1, "Statek 1", &seats_boat1, MAX_SEATS_BOAT1);

        // Odczyt pasażerów z FIFO_STATEK2
        process_fifo(fd_statek2, "Statek 2", &seats_boat2, MAX_SEATS_BOAT2);

        // Odczyt poleceń ze sternika dla Statek1
        char buffer_ster1[128];
        ssize_t n1 = read(fd_ster1, buffer_ster1, sizeof(buffer_ster1) - 1);
        if(n1 > 0){
            buffer_ster1[n1] = '\0';
            char czas[TIME_SIZE];
            getTime(czas);
            char log_msg[256];
            snprintf(log_msg, sizeof(log_msg),
                "[%s] Statek 1: Otrzymano polecenie od sternika: %s\n",
                czas, buffer_ster1);
            logStatek(log_msg);
            if(strncmp(buffer_ster1, "START", 5) == 0){
                if(seats_boat1 > 0){
                    // Rozpoczęcie rejsu
                    char log_start[256];
                    snprintf(log_start, sizeof(log_start),
                        "[%s] Statek 1: Rozpoczynam rejs z %d pasażerami.\n",
                        czas, seats_boat1);
                    logStatek(log_start);

                    // Symulacja rejsu
                    sleep(TRIP_DURATION);

                    snprintf(log_start, sizeof(log_start),
                        "[%s] Statek 1: Rejs zakończony. Pasażerowie mogą schodzić.\n",
                        czas);
                    logStatek(log_start);

                    // Reset liczby pasażerów
                    seats_boat1 = 0;
                    sharedCap[0] = seats_boat1;
                }
                else{
                    snprintf(log_msg, sizeof(log_msg),
                        "[%s] Statek 1: Brak pasażerów do rejsu.\n",
                        czas);
                    logStatek(log_msg);
                }
            }
        }

        // Odczyt poleceń ze sternika dla Statek2
        char buffer_ster2[128];
        ssize_t n2 = read(fd_ster2, buffer_ster2, sizeof(buffer_ster2) - 1);
        if(n2 > 0){
            buffer_ster2[n2] = '\0';
            char czas[TIME_SIZE];
            getTime(czas);
            char log_msg[256];
            snprintf(log_msg, sizeof(log_msg),
                "[%s] Statek 2: Otrzymano polecenie od sternika: %s\n",
                czas, buffer_ster2);
            logStatek(log_msg);
            if(strncmp(buffer_ster2, "START", 5) == 0){
                if(seats_boat2 > 0){
                    // Rozpoczęcie rejsu
                    char log_start[256];
                    snprintf(log_start, sizeof(log_start),
                        "[%s] Statek 2: Rozpoczynam rejs z %d pasażerami.\n",
                        czas, seats_boat2);
                    logStatek(log_start);

                    // Symulacja rejsu
                    sleep(TRIP_DURATION);

                    snprintf(log_start, sizeof(log_start),
                        "[%s] Statek 2: Rejs zakończony. Pasażerowie mogą schodzić.\n",
                        czas);
                    logStatek(log_start);

                    // Reset liczby pasażerów
                    seats_boat2 = 0;
                    sharedCap[1] = seats_boat2;
                }
                else{
                    snprintf(log_msg, sizeof(log_msg),
                        "[%s] Statek 2: Brak pasażerów do rejsu.\n",
                        czas);
                    logStatek(log_msg);
                }
            }
        }

        usleep(100000); // Czekaj 0.1 sekundy
    }

    // Zamknięcie FIFO
    close(fd_statek1);
    close(fd_statek2);
    close(fd_ster1);
    close(fd_ster2);

    cleanup(0);
    return 0;
}
