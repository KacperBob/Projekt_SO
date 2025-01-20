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

#define BOAT1_CAPACITY 10
#define BOAT2_CAPACITY 10
#define SHM_KEY 5678
#define TIME_SIZE 20

#define FIFO_BOAT1 "fifo_boat1"
#define FIFO_BOAT2 "fifo_boat2"
#define FIFO_POMOST "fifo_pomost"
#define FIFO_LOG "fifo_log"

float revenue = 0.0;
int boat1_available_seats = BOAT1_CAPACITY;
int boat2_available_seats = BOAT2_CAPACITY;

void cleanup(int signum) {
    (void)signum; // Ignorowanie parametru
    printf("\nZatrzymuję kasjera...\n");
    printf("Ostateczny przychód: %.2f PLN\n", revenue);
    unlink(FIFO_BOAT1);
    unlink(FIFO_BOAT2);
    unlink(FIFO_POMOST);
    unlink(FIFO_LOG);
    exit(0);
}

float calculate_ticket_cost(int age) {
    if (age < 3) {
        return 0.0;
    } else if (age >= 4 && age <= 14) {
        return 10.0; // Zniżka 50%
    } else {
        return 20.0;
    }
}

void send_to_fifo(const char *fifo_path, const char *message) {
    int fifo_fd = open(fifo_path, O_WRONLY | O_NONBLOCK);
    if (fifo_fd == -1) {
        return;
    }
    if (write(fifo_fd, message, strlen(message)) == -1) {
        close(fifo_fd);
        return;
    }
    close(fifo_fd);
}

void handle_passenger(const char *time, int pid, int age) {
    float cost = calculate_ticket_cost(age);
    const char *boat_assignment;
    char fifo_message[256];

    if (boat1_available_seats > 0) {
        boat1_available_seats--;
        boat_assignment = "Łódź nr 1";
        snprintf(fifo_message, sizeof(fifo_message), "Pasażer PID: %d, wiek: %d, statek: 1\n", pid, age);
        send_to_fifo(FIFO_BOAT1, fifo_message);
        printf("[%s] Kasjer: Pasażer (PID: %d, wiek: %d) zapłacił %.2f PLN. Przydział: Łódź nr 1. Pozostało miejsc: %d/10\n", time, pid, age, cost, boat1_available_seats);
    } else if (boat2_available_seats > 0) {
        boat2_available_seats--;
        boat_assignment = "Łódź nr 2";
        snprintf(fifo_message, sizeof(fifo_message), "Pasażer PID: %d, wiek: %d, statek: 2\n", pid, age);
        send_to_fifo(FIFO_BOAT2, fifo_message);
        printf("[%s] Kasjer: Pasażer (PID: %d, wiek: %d) zapłacił %.2f PLN. Przydział: Łódź nr 2. Pozostało miejsc: %d/10\n", time, pid, age, cost, boat2_available_seats);
    } else {
        printf("[%s] Kasjer: Pasażer (PID: %d, wiek: %d) odrzucony - brak miejsc.\n", time, pid, age);
        snprintf(fifo_message, sizeof(fifo_message), "Pasażer PID: %d, wiek: %d odrzucony - brak miejsc\n", pid, age);
        send_to_fifo(FIFO_LOG, fifo_message);
        return;
    }

    snprintf(fifo_message, sizeof(fifo_message), "[%s] Kasjer obsłużył pasażera PID: %d, wiek: %d, koszt: %.2f, statek: %s\n",
             time, pid, age, cost, boat_assignment);
    send_to_fifo(FIFO_POMOST, fifo_message);
}

void handle_passenger_with_dependant(const char *time, int parent_pid, int parent_age, int dependant_pid, int dependant_age) {
    float parent_cost = calculate_ticket_cost(parent_age);
    float dependant_cost = calculate_ticket_cost(dependant_age);
    float total_cost = parent_cost + dependant_cost;
    char fifo_message[256];

    if (boat2_available_seats >= 2) {
        boat2_available_seats -= 2;
        printf("[%s] Kasjer: Pasażerowie (PID rodzica: %d, wiek: %d, PID dziecka/seniora: %d, wiek: %d) zapłacili %.2f PLN. Przydział: Łódź nr 2. Pozostało miejsc: %d/10\n",
               time, parent_pid, parent_age, dependant_pid, dependant_age, total_cost, boat2_available_seats);
        snprintf(fifo_message, sizeof(fifo_message), "Rodzic PID: %d, wiek: %d; Dziecko/Senior PID: %d, wiek: %d, statek: 2\n",
                 parent_pid, parent_age, dependant_pid, dependant_age);
        send_to_fifo(FIFO_BOAT2, fifo_message);
        printf("Statek 2: Pasażer: Rodzic PID: %d, wiek: %d; Dziecko/Senior PID: %d, wiek: %d zajęli miejsce %d/10.\n",
               parent_pid, parent_age, dependant_pid, dependant_age, (BOAT2_CAPACITY - boat2_available_seats));
    } else {
        printf("[%s] Kasjer: Pasażerowie (PID rodzica: %d, PID dziecka/seniora: %d) odrzuceni - brak miejsc na łodzi nr 2.\n",
               time, parent_pid, dependant_pid);
        snprintf(fifo_message, sizeof(fifo_message), "Rodzic PID: %d, PID dziecka/seniora: %d odrzuceni - brak miejsc na łodzi nr 2\n",
                 parent_pid, dependant_pid);
        send_to_fifo(FIFO_LOG, fifo_message);
        return;
    }

    revenue += total_cost;
    snprintf(fifo_message, sizeof(fifo_message), "[%s] Kasjer obsłużył rodzinę: Rodzic PID: %d, wiek: %d; Dziecko/Senior PID: %d, wiek: %d, koszt: %.2f\n",
             time, parent_pid, parent_age, dependant_pid, dependant_age, total_cost);
    send_to_fifo(FIFO_POMOST, fifo_message);
}

void initialize_fifos() {
    if (mkfifo(FIFO_BOAT1, 0666) == -1 && errno != EEXIST) {
        exit(EXIT_FAILURE);
    }

    if (mkfifo(FIFO_BOAT2, 0666) == -1 && errno != EEXIST) {
        exit(EXIT_FAILURE);
    }

    if (mkfifo(FIFO_POMOST, 0666) == -1 && errno != EEXIST) {
        exit(EXIT_FAILURE);
    }

    if (mkfifo(FIFO_LOG, 0666) == -1 && errno != EEXIST) {
        exit(EXIT_FAILURE);
    }
}

int main() {
    struct sigaction sa;
    sa.sa_handler = cleanup;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    initialize_fifos();

    // Dodanie opóźnienia, aby inne procesy mogły poprawnie otworzyć FIFO
    sleep(1);

    int shmid;
    char *shared_time;

    // Uzyskanie dostępu do pamięci współdzielonej
    shmid = shmget(SHM_KEY, TIME_SIZE, 0666);
    while (shmid == -1) {
        sleep(1);
        shmid = shmget(SHM_KEY, TIME_SIZE, 0666);
    }

    // Dołączenie segmentu pamięci współdzielonej
    shared_time = (char *)shmat(shmid, NULL, 0);
    if (shared_time == (char *)-1) {
        exit(EXIT_FAILURE);
    }

    srand(time(NULL));

    while (1) {
        // Symulowany czas pobierany z pamięci współdzielonej
        char simulated_time[TIME_SIZE];
        strncpy(simulated_time, shared_time, TIME_SIZE);
        simulated_time[TIME_SIZE - 1] = '\0'; // Upewnienie się, że czas jest zakończony \0

        int mode = rand() % 2; // Wybór między pasażerem pojedynczym a z osobą zależną
        if (mode == 0) {
            // Pojedynczy pasażer
            int pid = rand() % 10000 + 1; // Losowy PID
            int age = rand() % 56 + 15;  // Wiek: 15-70
            handle_passenger(simulated_time, pid, age);
        } else {
            // Pasażer z osobą zależną
            int parent_pid = rand() % 10000 + 1;    // Losowy PID rodzica
            int dependant_pid = rand() % 10000 + 1; // Losowy PID dziecka/seniora
            int parent_age = rand() % 56 + 15;      // Wiek rodzica: 15-70
            int dependant_age = (rand() % 2 == 0) ? (rand() % 14 + 1) : (rand() % 30 + 71); // Wiek dziecka: 1-14, seniora: 71-100
            handle_passenger_with_dependant(simulated_time, parent_pid, parent_age, dependant_pid, dependant_age);
        }

        sleep(1); // Symulacja przetwarzania
    }

    // Odłączenie segmentu pamięci współdzielonej
    if (shmdt(shared_time) == -1) {
        exit(EXIT_FAILURE);
    }

    return 0;
}
