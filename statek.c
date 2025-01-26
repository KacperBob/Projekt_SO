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

#define MAX_SEATS_BOAT1 7  // Liczba miejsc na statku 1
#define MAX_SEATS_BOAT2 4  // Liczba miejsc na statku 2
#define TRIP_DURATION   5  // Czas trwania rejsu w sekundach
#define TIME_LIMIT      20 // Limit czasu oczekiwania na pasażera w sekundach

#define FIFO_STATEK1     "fifo_statek1"
#define FIFO_STATEK2     "fifo_statek2"
#define FIFO_STERNIK1    "fifo_ster1"
#define FIFO_STERNIK2    "fifo_ster2"

int shmid_time = -1;
char *shared_time = NULL;
int shmid_capacities = -1;
int *sharedCap = NULL;

int seats_boat1 = 0;
int seats_boat2 = 0;
time_t last_passenger_time_boat1;
time_t last_passenger_time_boat2;

volatile sig_atomic_t running = 1;

void cleanup(int signum) {
    if (shared_time != NULL) {
        shmdt(shared_time);
    }
    if (sharedCap != NULL) {
        shmdt(sharedCap);
    }
    printf("\nStatek zamknięty.\n");
    exit(0);
}

void getTime(char *b) {
    if (shared_time == NULL) {
        // Użyj systemowego czasu jako fallback
        time_t now = time(NULL);
        struct tm *tm_info = localtime(&now);
        strftime(b, TIME_SIZE, "%H:%M:%S", tm_info);
        return;
    }
    snprintf(b, TIME_SIZE, "%s", shared_time);
}

void logStatek(const char *s) {
    int f = open("statekraport.txt", O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (f >= 0) {
        write(f, s, strlen(s));
        close(f);
    }
    printf("%s", s);
    fflush(stdout);
}

void notify_sternik(const char *fifo_name, const char *message) {
    int fd = open(fifo_name, O_WRONLY | O_NONBLOCK);
    if (fd >= 0) {
        write(fd, message, strlen(message));
        close(fd);
    } else {
        perror("statek: Nie można otworzyć FIFO dla sternika");
    }
}

void process_fifo(int fd, const char *boat_name, int *current_seats, int max_seats, const char *sternik_fifo, time_t *last_passenger_time) {
    char buffer[128];
    ssize_t bytes_read;
    while ((bytes_read = read(fd, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[bytes_read] = '\0'; // Null-terminate
        char czas[TIME_SIZE];
        getTime(czas);

        int pid, boat_num, free_seats;
        if (sscanf(buffer, "%d:%d:%d", &pid, &boat_num, &free_seats) != 3 || 
            free_seats < 0 || free_seats > max_seats) {
            char log_msg[256];
            snprintf(log_msg, sizeof(log_msg),
                "[%s] statek: Niepoprawny format komunikatu lub nieprawidłowa liczba miejsc: %d dla statku %d.\n",
                czas, free_seats, boat_num);
            logStatek(log_msg);
            continue; // Pomijamy błędny komunikat
        }

        *current_seats = free_seats;
        *last_passenger_time = time(NULL);

        char log_msg[256];
        snprintf(log_msg, sizeof(log_msg),
            "[%s] statek: pasazer pid: %d. wsiadl na statek %d. Liczba miejsc na statku: %d.\n",
            czas, pid, boat_num, free_seats);
        logStatek(log_msg);

        if (*current_seats == max_seats) {
            char notify_msg[128];
            snprintf(notify_msg, sizeof(notify_msg), "Statek %d jest pełny. Rozpocznij rejs.\n", boat_num);
            notify_sternik(sternik_fifo, notify_msg);

            snprintf(log_msg, sizeof(log_msg),
                "[%s] statek: Powiadomiono sternika o pełnym statku %d.\n",
                czas, boat_num);
            logStatek(log_msg);
        }
    }

    if (bytes_read == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
        perror("statek: read");
    }
}

void check_and_start_trip(const char *boat_name, int *current_seats, int max_seats, time_t *last_passenger_time, const char *sternik_fifo) {
    time_t current_time = time(NULL);
    double time_diff = difftime(current_time, *last_passenger_time);

    if (*current_seats > 0 && time_diff >= TIME_LIMIT) {
        char notify_msg[128];
        snprintf(notify_msg, sizeof(notify_msg), "Statek %s: Limit czasu minął. Rozpocznij rejs.\n", boat_name);
        notify_sternik(sternik_fifo, notify_msg);

        char log_msg[256];
        snprintf(log_msg, sizeof(log_msg),
            "[%s] statek: Limit czasu dla %s minął. Powiadomiono sternika o rozpoczęciu rejsu.\n",
            boat_name, boat_name);
        logStatek(log_msg);
    }
}

int main() {
    signal(SIGINT, cleanup);
    signal(SIGTERM, cleanup);

    // Łączenie z pamięcią współdzieloną dla czasu
    shmid_time = shmget(SHM_KEY_TIME, TIME_SIZE, 0666);
    while (shmid_time == -1) {
        perror("statek: shmget czas");
        sleep(1);
        shmid_time = shmget(SHM_KEY_TIME, TIME_SIZE, 0666);
    }
    shared_time = shmat(shmid_time, NULL, 0);
    if (shared_time == (char *)-1) {
        perror("statek: shmat czas");
        cleanup(0);
    }

    // Łączenie z pamięcią współdzieloną dla pojemności statków
    shmid_capacities = shmget(SHM_KEY_CAPACITY, 2 * sizeof(int), IPC_CREAT | 0666);
    if (shmid_capacities == -1) {
        perror("statek: shmget capacity");
        cleanup(0);
    }
    sharedCap = shmat(shmid_capacities, NULL, 0);
    if (sharedCap == (int *)-1) {
        perror("statek: shmat capacity");
        cleanup(0);
    }

    // Inicjalizacja pamięci współdzielonej
    if (sharedCap[0] == 0 && sharedCap[1] == 0) {
        sharedCap[0] = MAX_SEATS_BOAT1; // 7 miejsc na statek 1
        sharedCap[1] = MAX_SEATS_BOAT2; // 4 miejsca na statek 2
    }

    seats_boat1 = sharedCap[0];
    seats_boat2 = sharedCap[1];
    time(&last_passenger_time_boat1);
    time(&last_passenger_time_boat2);

    // Tworzenie FIFO
    if (mkfifo(FIFO_STATEK1, 0666) == -1 && errno != EEXIST) {
        perror("statek: mkfifo_statek1");
        cleanup(0);
    }
    if (mkfifo(FIFO_STATEK2, 0666) == -1 && errno != EEXIST) {
        perror("statek: mkfifo_statek2");
        cleanup(0);
    }
    if (mkfifo(FIFO_STERNIK1, 0666) == -1 && errno != EEXIST) {
        perror("statek: mkfifo_sternik1");
        cleanup(0);
    }
    if (mkfifo(FIFO_STERNIK2, 0666) == -1 && errno != EEXIST) {
        perror("statek: mkfifo_sternik2");
        cleanup(0);
    }

    printf("Statek uruchomiony. Czeka na pasażerów i polecenia sternika.\n");
    logStatek("Statek uruchomiony. Czeka na pasażerów i polecenia sternika.\n");

    // Otwieranie FIFO w trybie nieblokującym
    int fd_statek1 = open(FIFO_STATEK1, O_RDONLY | O_NONBLOCK);
    if (fd_statek1 == -1) {
        perror("statek: open fifo_statek1");
        cleanup(0);
    }
    int fd_statek2 = open(FIFO_STATEK2, O_RDONLY | O_NONBLOCK);
    if (fd_statek2 == -1) {
        perror("statek: open fifo_statek2");
        cleanup(0);
    }

    while (running) {
        process_fifo(fd_statek1, "Statek 1", &seats_boat1, MAX_SEATS_BOAT1, FIFO_STERNIK1, &last_passenger_time_boat1);
        process_fifo(fd_statek2, "Statek 2", &seats_boat2, MAX_SEATS_BOAT2, FIFO_STERNIK2, &last_passenger_time_boat2);

        check_and_start_trip("Statek 1", &seats_boat1, MAX_SEATS_BOAT1, &last_passenger_time_boat1, FIFO_STERNIK1);
        check_and_start_trip("Statek 2", &seats_boat2, MAX_SEATS_BOAT2, &last_passenger_time_boat2, FIFO_STERNIK2);

        usleep(100000); // Czekaj 0.1 sekundy
    }

    close(fd_statek1);
    close(fd_statek2);

    cleanup(0);
    return 0;
}
