#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <signal.h>
#include <string.h>
#include <errno.h>

#define SHM_KEY 5678
#define MSG_KEY 1234
#define BOAT1_CAPACITY 10
#define BOAT2_CAPACITY 8
#define BLOCKED_PID 2496

struct msgbuf {
    long mtype;
    char mtext[100];
};

float revenue = 0.0;
int boat1_available_seats = BOAT1_CAPACITY;
int boat2_available_seats = BOAT2_CAPACITY;

int msqid = -1;
int shmid = -1;
char *czas = NULL;
int blocked_pid_logged = 0; // Dodane: flaga do sprawdzania, czy PID był już zgłoszony

void cleanup(int signum) {
    printf("\nZwalnianie zasob\u00f3w...\n");

    if (czas != NULL) {
        if (shmdt(czas) == -1) {
            perror("Nie mo\u017cna od\u0142\u0105czy\u0107 pami\u0119ci wsp\u00f3\u0142dzielonej");
        }
    }

    if (msqid != -1) {
        if (msgctl(msqid, IPC_RMID, NULL) == -1) {
            perror("Nie mo\u017cna usun\u0105\u0107 kolejki komunikat\u00f3w");
        }
    }

    exit(0);
}

void get_simulated_time(char *buffer) {
    shmid = shmget(SHM_KEY, sizeof(char) * 20, 0666);
    if (shmid == -1) {
        perror("Nie mo\u017cna po\u0142\u0105czy\u0107 si\u0119 z pami\u0119ci\u0105 wsp\u00f3\u0142dzielon\u0105");
        cleanup(0);
    }

    czas = shmat(shmid, NULL, 0);
    if (czas == (char *)-1) {
        perror("Nie mo\u017cna przy\u0142\u0105czy\u0107 pami\u0119ci wsp\u00f3\u0142dzielonej");
        cleanup(0);
    }

    snprintf(buffer, 20, "%s", czas);

    if (shmdt(czas) == -1) {
        perror("Nie mo\u017cna od\u0142\u0105czy\u0107 pami\u0119ci wsp\u00f3\u0142dzielonej po odczycie");
    }
    czas = NULL;
}

int is_within_operating_hours(const char *time) {
    int hour, minute, second;
    sscanf(time, "%d:%d:%d", &hour, &minute, &second);
    return (hour >= 6 && hour < 23);
}

float calculate_ticket_cost(int age) {
    if (age < 3) {
        return 0.0;
    } else {
        return 20.0;
    }
}

const char* assign_boat(int age, int group_with_adult, int *seats_needed) {
    if (age < 15) {
        if (group_with_adult) {
            if (boat2_available_seats >= *seats_needed) {
                boat2_available_seats -= *seats_needed;
                return "\u0142\u00f3d\u017a nr 2 (z opiekunem)";
            } else {
                return "Odrzucony (brak miejsc na \u0142odzi nr 2 dla grupy)";
            }
        } else {
            return "Odrzucony (wymagana opieka osoby doros\u0142ej)";
        }
    } else if (age > 70) {
        if (boat2_available_seats >= 1) {
            boat2_available_seats--;
            return "\u0142\u00f3d\u017a nr 2";
        } else {
            return "Odrzucony (brak miejsc na \u0142odzi nr 2)";
        }
    } else {
        if (boat1_available_seats >= 1) {
            boat1_available_seats--;
            return "\u0142\u00f3d\u017a nr 1";
        } else if (boat2_available_seats >= 1) {
            boat2_available_seats--;
            return "\u0142\u00f3d\u017a nr 2";
        } else {
            return "Odrzucony (brak miejsc na obu \u0142odziach)";
        }
    }
}

int main() {
    signal(SIGINT, cleanup);
    signal(SIGTERM, cleanup);

    msqid = msgget(MSG_KEY, IPC_CREAT | 0666);
    if (msqid == -1) {
        perror("Nie mo\u017cna utworzy\u0107 kolejki komunikat\u00f3w");
        exit(1);
    }

    struct msgbuf msg;
    char simulated_time[20];
    int last_processed_pid = -1;

    while (1) {
        get_simulated_time(simulated_time);

        if (!is_within_operating_hours(simulated_time)) {
            printf("[%s] Kasjer: Zamkni\u0119te, otwarte od 6:00.\n", simulated_time);
            sleep(1);
            continue;
        }

        if (msgrcv(msqid, &msg, sizeof(msg.mtext), 1, IPC_NOWAIT) == -1) {
            if (errno != ENOMSG) {
                perror("B\u0142\u0105d podczas odbioru wiadomo\u015bci");
            }
            sleep(1);
            continue;
        }

        int age;
        int passenger_id;
        int group_with_adult = 0;
        int seats_needed = 1;
        sscanf(msg.mtext, "Pasa\u017cer (wiek: %d, PID: %d)", &age, &passenger_id);

        if (passenger_id == BLOCKED_PID) {
            if (!blocked_pid_logged) {
                printf("[%s] Kasjer: Pasa\u017cer o PID: %d jest zablokowany i nie b\u0119dzie obs\u0142ugiwany.\n", simulated_time, passenger_id);
                blocked_pid_logged = 1;
            }
            continue;
        }

        if (passenger_id == last_processed_pid) {
            sleep(1);
            continue;
        }

        last_processed_pid = passenger_id;

        if (age < 15) {
            group_with_adult = 1;
            seats_needed = 2;
        }

        float cost = calculate_ticket_cost(age);
        const char* boat_assignment = assign_boat(age, group_with_adult, &seats_needed);

        if (cost == 0.0) {
            printf("[%s] Kasjer: Pasa\u017cer (PID: %d, wiek: %d) otrzyma\u0142 darmowy bilet. Przydzia\u0142: %s.\n", simulated_time, passenger_id, age, boat_assignment);
        } else {
            printf("[%s] Kasjer: Pasa\u017cer (PID: %d, wiek: %d) zap\u0142aci\u0142 %.2f PLN. Przydzia\u0142: %s.\n", simulated_time, passenger_id, age, cost, boat_assignment);
            revenue += cost;
        }
    }

    return 0;
}
