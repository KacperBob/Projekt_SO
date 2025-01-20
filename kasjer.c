#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <errno.h>

#define SHM_KEY 5678
#define SHM_KEY_CAPACITY 8765
#define TIME_SIZE 20
#define FIFO_BOAT1 "fifo_boat1"
#define FIFO_BOAT2 "fifo_boat2"
#define FIFO_POMOST "fifo_pomost"
#define FIFO_LOG "fifo_log"

int *shared_capacities;
int boat1_available_seats;
int boat2_available_seats;
float revenue = 0.0;

void cleanup(int signum) {
    printf("\nZatrzymuję kasjera...\n");
    printf("Ostateczny przychód: %.2f PLN\n", revenue);
    unlink(FIFO_BOAT1);
    unlink(FIFO_BOAT2);
    unlink(FIFO_POMOST);
    unlink(FIFO_LOG);
    if (shared_capacities != NULL && shmdt(shared_capacities) == -1) {
        perror("Nie można odłączyć pamięci współdzielonej dla pojemności");
    }
    exit(0);
}

float calculate_ticket_cost(int age) {
    if (age < 3) {
        return 0.0;
    } else if (age >= 4 && age <= 14) {
        return 10.0;
    } else {
        return 20.0;
    }
}

void send_to_fifo(const char *fifo_path, const char *message) {
    int fifo_fd = open(fifo_path, O_WRONLY | O_NONBLOCK);
    if (fifo_fd == -1) return;
    write(fifo_fd, message, strlen(message));
    close(fifo_fd);
}

void handle_passenger(const char *time, int pid, int age) {
    float cost = calculate_ticket_cost(age);
    char fifo_message[256];

    if (boat1_available_seats > 0) {
        boat1_available_seats--;
        snprintf(fifo_message, sizeof(fifo_message), "Pasażer PID: %d, wiek: %d, statek: 1\n", pid, age);
        send_to_fifo(FIFO_BOAT1, fifo_message);
        printf("[%s] Kasjer: Pasażer (PID: %d, wiek: %d) zapłacił %.2f PLN. Przydział: Łódź nr 1. Pozostało miejsc: %d/%d\n", time, pid, age, cost, boat1_available_seats, shared_capacities[0]);
    } else if (boat2_available_seats > 0) {
        boat2_available_seats--;
        snprintf(fifo_message, sizeof(fifo_message), "Pasażer PID: %d, wiek: %d, statek: 2\n", pid, age);
        send_to_fifo(FIFO_BOAT2, fifo_message);
        printf("[%s] Kasjer: Pasażer (PID: %d, wiek: %d) zapłacił %.2f PLN. Przydział: Łódź nr 2. Pozostało miejsc: %d/%d\n", time, pid, age, cost, boat2_available_seats, shared_capacities[1]);
    } else {
        printf("[%s] Kasjer: Pasażer (PID: %d, wiek: %d) odrzucony - brak miejsc.\n", time, pid, age);
        snprintf(fifo_message, sizeof(fifo_message), "Pasażer PID: %d, wiek: %d odrzucony - brak miejsc\n", pid, age);
        send_to_fifo(FIFO_LOG, fifo_message);
    }
    revenue += cost;
}

void initialize_fifos() {
    mkfifo(FIFO_BOAT1, 0666);
    mkfifo(FIFO_BOAT2, 0666);
    mkfifo(FIFO_POMOST, 0666);
    mkfifo(FIFO_LOG, 0666);
}

void read_boat_capacities() {
    int shmid_capacities = shmget(SHM_KEY_CAPACITY, 2 * sizeof(int), 0666);
    while (shmid_capacities == -1) {
        perror("Nie można uzyskać dostępu do pamięci współdzielonej dla pojemności");
        sleep(1); // Oczekiwanie na utworzenie pamięci współdzielonej przez statek
        shmid_capacities = shmget(SHM_KEY_CAPACITY, 2 * sizeof(int), 0666);
    }

    shared_capacities = (int *)shmat(shmid_capacities, NULL, 0);
    if (shared_capacities == (int *)-1) {
        perror("Nie można przyłączyć pamięci współdzielonej dla pojemności");
        exit(1);
    }

    boat1_available_seats = shared_capacities[0];
    boat2_available_seats = shared_capacities[1];
}

int main() {
    struct sigaction sa;
    sa.sa_handler = cleanup;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    read_boat_capacities();
    initialize_fifos();

    sleep(1);

    int shmid;
    char *shared_time;
    shmid = shmget(SHM_KEY, TIME_SIZE, 0666);
    if (shmid == -1) {
        perror("Nie można uzyskać dostępu do pamięci współdzielonej dla czasu");
        exit(1);
    }

    shared_time = (char *)shmat(shmid, NULL, 0);
    if (shared_time == (char *)-1) {
        perror("Nie można przyłączyć pamięci współdzielonej dla czasu");
        exit(1);
    }

    srand(time(NULL));

    while (1) {
        char simulated_time[TIME_SIZE];
        strncpy(simulated_time, shared_time, TIME_SIZE);
        simulated_time[TIME_SIZE - 1] = '\0';

        int pid = rand() % 10000 + 1;
        int age = rand() % 80 + 1;
        handle_passenger(simulated_time, pid, age);

        sleep(1);
    }

    if (shmdt(shared_time) == -1) {
        perror("Nie można odłączyć pamięci współdzielonej dla czasu");
        exit(1);
    }

    return 0;
}
