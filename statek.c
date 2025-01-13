#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <sys/stat.h>


#define FIFO_PATH1 "fifo_boat1"
#define FIFO_PATH2 "fifo_boat2"

void cleanup() {
    unlink(FIFO_PATH1);
    unlink(FIFO_PATH2);
    printf("FIFO files removed.\n");
    exit(0);
}

void handle_signal(int sig) {
    cleanup();
}

int main() {
    // Tworzenie FIFO
    if (mkfifo(FIFO_PATH1, 0666) == -1 && errno != EEXIST) {
        perror("Nie udało się utworzyć FIFO dla boat1");
        exit(1);
    }

    if (mkfifo(FIFO_PATH2, 0666) == -1 && errno != EEXIST) {
        perror("Nie udało się utworzyć FIFO dla boat2");
        unlink(FIFO_PATH1);
        exit(1);
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
    }

    int fd_boat2 = open(FIFO_PATH2, O_RDONLY);
    if (fd_boat2 == -1) {
        perror("Nie udało się otworzyć FIFO dla boat2");
        cleanup();
    }

    printf("FIFO files opened for reading.\n");

    char buffer[256];
    while (1) {
        // Odczyt z FIFO dla boat1
        memset(buffer, 0, sizeof(buffer));
        ssize_t bytes_read = read(fd_boat1, buffer, sizeof(buffer) - 1);
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0'; // Null-terminate the string
            printf("[BOAT 1]: %s\n", buffer);
        }

        // Odczyt z FIFO dla boat2
        memset(buffer, 0, sizeof(buffer));
        bytes_read = read(fd_boat2, buffer, sizeof(buffer) - 1);
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0'; // Null-terminate the string
            printf("[BOAT 2]: %s\n", buffer);
        }

        usleep(100000); // Czas oczekiwania na kolejne dane
    }

    close(fd_boat1);
    close(fd_boat2);
    cleanup();

    return 0;
}
