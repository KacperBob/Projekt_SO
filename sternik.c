#include <stdio.h>
#include <stdlib.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <unistd.h>
#include "common.h"

/*
 * Program sternik.c – monitoruje stan obłożenia statków i wysyła komunikaty "depart"
 * (przez kolejkę komunikatów) dla statków, gdy liczba pasażerów zgromadzonych na danym statku
 * osiągnie pełną pojemność. Aby statki mogły odpłynąć w różnych momentach, sternik dzieli się
 * na dwa oddzielne procesy: jeden monitoruje statek 1, a drugi statek 2.
 */

int main() {
    // Uzyskujemy kolejkę komunikatów do wysyłania komunikatów "depart"
    int msq_depart = msgget(MSG_KEY_DEPART, 0666 | IPC_CREAT);
    if (msq_depart == -1) {
        perror("msgget (depart) in sternik");
        exit(EXIT_FAILURE);
    }

    // Uzyskujemy dostęp do pamięci dzielonej, w której przechowywany jest stan statków (struktura boats_shm_t)
    int shmid = shmget(SHM_KEY_BOATS, sizeof(boats_shm_t), 0666 | IPC_CREAT);
    if (shmid == -1) {
        perror("shmget (boats) in sternik");
        exit(EXIT_FAILURE);
    }
    boats_shm_t *boats = (boats_shm_t *) shmat(shmid, NULL, 0);
    if (boats == (void *) -1) {
        perror("shmat (boats) in sternik");
        exit(EXIT_FAILURE);
    }

    /*
     * Rozdzielamy pracę sternika na dwa procesy:
     * - Proces 1 monitoruje stan obłożenia statku nr 1.
     * - Proces 2 monitoruje stan obłożenia statku nr 2.
     * Każdy z nich działa w nieskończonej pętli i sprawdza, czy dany statek
     * jest otwarty na przyjmowanie pasażerów (boatX_boarding_open) oraz czy liczba pasażerów
     * (occupancy_boatX) osiągnęła pojemność statku (BOATX_CAPACITY).
     */
    pid_t pid1 = fork();
    if (pid1 < 0) {
        perror("fork");
        exit(EXIT_FAILURE);
    }
    if (pid1 == 0) {
        // Proces potomny dla statku nr 1
        while (1) {
            // Krótka przerwa, aby nie obciążać CPU
            usleep(500000); // 500 milisekund

            // Sprawdzamy, czy statek 1 jest otwarty na przyjmowanie pasażerów i czy liczba pasażerów
            // osiągnęła pełną pojemność.
            if (boats->boat1_boarding_open && boats->occupancy_boat1 >= BOAT1_CAPACITY) {
                depart_msg dmsg;
                dmsg.mtype = 1; // mtype = 1 – komunikat dla statku nr 1

                // Wysyłamy komunikat "depart" dla statku 1
                if (msgsnd(msq_depart, &dmsg, sizeof(dmsg) - sizeof(long), 0) == -1) {
                    perror("msgsnd (depart) for ship 1 in sternik");
                } else {
                    printf("[Sternik] Wysłano sygnał do statku nr 1 (depart), zgromadzono %d pasażerów.\n", boats->occupancy_boat1);
                }

                // Po wysłaniu komunikatu czekamy, aż stan obłożenia statku 1 zostanie zresetowany
                // (czyli pasażerowie opuszczą statek i occupancy_boat1 zostanie ustawione na 0).
                while (boats->occupancy_boat1 != 0) {
                    usleep(500000);
                }
            }
        }
        // Zakończenie procesu potomnego dla statku 1 (teoretycznie nigdy się nie zdarzy)
        exit(0);
    }

    // Proces macierzysty tworzy kolejny proces potomny dla statku nr 2.
    pid_t pid2 = fork();
    if (pid2 < 0) {
        perror("fork");
        exit(EXIT_FAILURE);
    }
    if (pid2 == 0) {
        // Proces potomny dla statku nr 2
        while (1) {
            usleep(500000); // 500 milisekund

            // Sprawdzamy, czy statek 2 jest otwarty na przyjmowanie pasażerów i czy liczba pasażerów
            // osiągnęła pojemność statku 2.
            if (boats->boat2_boarding_open && boats->occupancy_boat2 >= BOAT2_CAPACITY) {
                depart_msg dmsg;
                dmsg.mtype = 2; // mtype = 2 – komunikat dla statku nr 2

                // Wysyłamy komunikat "depart" dla statku 2
                if (msgsnd(msq_depart, &dmsg, sizeof(dmsg) - sizeof(long), 0) == -1) {
                    perror("msgsnd (depart) for ship 2 in sternik");
                } else {
                    printf("[Sternik] Wysłano sygnał do statku nr 2 (depart), zgromadzono %d pasażerów.\n", boats->occupancy_boat2);
                }

                // Czekamy, aż stan obłożenia statku 2 zostanie zresetowany (occupancy_boat2 == 0)
                while (boats->occupancy_boat2 != 0) {
                    usleep(500000);
                }
            }
        }
        exit(0);
    }

    /*
     * Proces macierzysty (sternik główny) może teraz wykonywać inne zadania lub po prostu
     * czekać – tutaj stosujemy długi sen, ponieważ kontrola nad statkami odbywa się w procesach potomnych.
     */
    while (1) {
        sleep(60);
    }

    // Odłączamy pamięć dzieloną (teoretycznie, nigdy się nie wykonuje)
    shmdt(boats);
    return 0;
}
