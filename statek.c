k#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <string.h>

#define SHM_KEY1 12345
#define SHM_KEY2 12346
#define FIFO_PATH1 "./fifo_boat1"
#define FIFO_PATH2 "./fifo_boat2"
#define BOAT1_CAPACITY 70
#define BOAT2_CAPACITY 50
#define TIMEOUT 20  // 20 minutes without passengers
#define TRIP_TIME 120 // 2 hours trip time

struct boat_state {
    int occupied_seats;
    pid_t passengers[70]; // Max capacity for boat 1
};

int shmid1, shmid2;
struct boat_state *boat1, *boat2;

void cleanup(int signum) {
    printf("\nCleaning up resources...\n");

    if (boat1 != NULL && shmdt(boat1) == -1) {
        perror("Failed to detach shared memory for boat 1");
    }
    if (shmid1 != -1 && shmctl(shmid1, IPC_RMID, NULL) == -1) {
        perror("Failed to remove shared memory for boat 1");
    }

    if (boat2 != NULL && shmdt(boat2) == -1) {
        perror("Failed to detach shared memory for boat 2");
    }
    if (shmid2 != -1 && shmctl(shmid2, IPC_RMID, NULL) == -1) {
        perror("Failed to remove shared memory for boat 2");
    }
    unlink(FIFO_PATH1);
    unlink(FIFO_PATH2);

    exit(0);
}

void init_shared_memory() {
    shmid1 = shmget(SHM_KEY1, sizeof(struct boat_state), IPC_CREAT | 0666);
    shmid2 = shmget(SHM_KEY2, sizeof(struct boat_state), IPC_CREAT | 0666);
    if (shmid1 == -1 || shmid2 == -1) {
        perror("Failed to create shared memory");
        exit(1);
    }

    boat1 = (struct boat_state *)shmat(shmid1, NULL, 0);
    boat2 = (struct boat_state *)shmat(shmid2, NULL, 0);
    if (boat1 == (void *)-1 || boat2 == (void *)-1) {
        perror("Failed to attach shared memory");
        cleanup(0);
    }

    memset(boat1, 0, sizeof(struct boat_state));
    memset(boat2, 0, sizeof(struct boat_state));
}

void create_fifos() {
    // Usuń istniejące FIFO, jeśli istnieją
    unlink(FIFO_PATH1);
    unlink(FIFO_PATH2);

    // Twórz nowe FIFO
    if (mkfifo(FIFO_PATH1, 0666) == -1) {
        perror("Failed to create FIFO for Boat 1");
        cleanup(0);
    }

    if (mkfifo(FIFO_PATH2, 0666) == -1) {
        perror("Failed to create FIFO for Boat 2");
        cleanup(0);
    }

    printf("FIFOs created: %s, %s\n", FIFO_PATH1, FIFO_PATH2);
}

void handle_boat(const char *fifo_path, struct boat_state *boat, int capacity, int delay_between_trips) {
    int fifo_fd;
    char buffer[128];
    time_t last_passenger_time = time(NULL);

    while (1) {
        fifo_fd = open(fifo_path, O_RDONLY);
        if (fifo_fd == -1) {
            perror("Failed to open FIFO");
            cleanup(0);
        }

        while (read(fifo_fd, buffer, sizeof(buffer)) > 0) {
            pid_t passenger_pid;
            sscanf(buffer, "%d", &passenger_pid);

            if (boat->occupied_seats < capacity) {
                boat->passengers[boat->occupied_seats++] = passenger_pid;
                printf("Passenger %d boarded the boat (%s). Seats occupied: %d/%d\n", passenger_pid, fifo_path, boat->occupied_seats, capacity);

                last_passenger_time = time(NULL);

                if (boat->occupied_seats == capacity) {
                    printf("Boat (%s) is fully loaded. Departing...\n", fifo_path);
                    break;
                }
            } else {
                printf("Passenger %d denied boarding. Boat (%s) is full.\n", passenger_pid, fifo_path);
            }
        }

        close(fifo_fd);

        if (difftime(time(NULL), last_passenger_time) >= TIMEOUT * 60 || boat->occupied_seats == capacity) {
            printf("Boat (%s) departing with %d passengers.\n", fifo_path, boat->occupied_seats);
            sleep(TRIP_TIME * 60); // Simulate trip
            printf("Boat (%s) returned from trip.\n", fifo_path);

            memset(boat, 0, sizeof(struct boat_state));
            sleep(delay_between_trips * 60);
        }
    }
}

int main() {
    signal(SIGINT, cleanup);
    signal(SIGTERM, cleanup);

    init_shared_memory();
    create_fifos();

    if (fork() == 0) {
        // Process for Boat 1
        handle_boat(FIFO_PATH1, boat1, BOAT1_CAPACITY, 30);
        exit(0);
    }

    if (fork() == 0) {
        // Process for Boat 2
        handle_boat(FIFO_PATH2, boat2, BOAT2_CAPACITY, 30);
        exit(0);
    }

    while (1) {
        pause(); // Keep the parent process running to handle signals
    }

    return 0;
}

