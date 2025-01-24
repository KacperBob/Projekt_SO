// kasjer.c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/msg.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#define SHM_KEY_TIME       5678
#define SHM_KEY_CAPACITY   8765 // Pamięć współdzielona z `statek.c`
#define TIME_SIZE          20
#define MSG_KEY            1234

#define FIFO_POMOST        "fifo_pomost"
#define FIFO_STATEK1       "fifo_statek1"
#define FIFO_STATEK2       "fifo_statek2"
#define FIFO_STER1         "fifo_ster1"
#define FIFO_STER2         "fifo_ster2"
#define FIFO_LOG           "fifo_log"
#define FIFO_STERNIK       "fifo_sternik"  // Komunikacja ze sternikiem
#define FIFO_POLICJA       "fifo_policja"  // Komunikacja z policją

struct msgbuf {
    long mtype;
    char mtext[128];
};

int msqid = -1;
int shmidCap = -1;
int *sharedCap = NULL;
char *shared_time = NULL;
int shmidTime = -1;

float revenue = 0.0f;

void finish(int s){
    if(msqid >= 0){
        msgctl(msqid, IPC_RMID, NULL);
    }
    if(sharedCap != NULL){
        shmdt(sharedCap);
    }
    if(shared_time != NULL){
        shmdt(shared_time);
    }
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

void sendFifo(const char *msg, const char *fifo){
    int fd = open(fifo, O_WRONLY | O_NONBLOCK);
    if(fd >= 0){
        write(fd, msg, strlen(msg));
        close(fd);
    }
}

void logKasjer(const char *s){
    int f = open("kasjerraport.txt", O_WRONLY | O_CREAT | O_APPEND, 0666);
    if(f >= 0){
        write(f, s, strlen(s));
        close(f);
    }
    printf("%s", s);
    fflush(stdout);
}

float ticketPrice(int age){
    if(age <= 3)  return 0.0f;
    if(age <= 14) return 10.0f;
    if(age <= 100)return 20.0f;
    return 20.0f;
}

int main(){
    signal(SIGINT, finish);
    signal(SIGTERM, finish);

    // Łączenie z kolejką komunikatów
    msqid = msgget(MSG_KEY, IPC_CREAT | 0666);
    if(msqid < 0){
        perror("kasjer: msgget");
        exit(1);
    }

    // Tworzenie lub łączenie się z pamięcią współdzieloną dla czasu
    shmidTime = shmget(SHM_KEY_TIME, TIME_SIZE, IPC_CREAT | 0666);
    if(shmidTime == -1){
        perror("kasjer: shmget czas");
        finish(0);
    }
    shared_time = shmat(shmidTime, NULL, 0);
    if(shared_time == (char*)-1){
        perror("kasjer: shmat czas");
        finish(0);
    }

    // Tworzenie lub łączenie się z pamięcią współdzieloną dla pojemności statków
    // Próba utworzenia nowej pamięci współdzielonej
    shmidCap = shmget(SHM_KEY_CAPACITY, 2 * sizeof(int), IPC_CREAT | IPC_EXCL | 0666);
    if(shmidCap == -1){
        if(errno == EEXIST){
            // Pamięć już istnieje, łączymy się z nią
            shmidCap = shmget(SHM_KEY_CAPACITY, 2 * sizeof(int), 0666);
            if(shmidCap == -1){
                perror("kasjer: shmget capacity");
                finish(0);
            }
        }
        else{
            perror("kasjer: shmget capacity");
            finish(0);
        }
    }
    else{
        // Pamięć została utworzona teraz, inicjalizujemy
        sharedCap = (int*)shmat(shmidCap, NULL, 0);
        if(sharedCap == (int*)-1){
            perror("kasjer: shmat capacity");
            finish(0);
        }
        sharedCap[0] = 4; // MAX_SEATS_BOAT1
        sharedCap[1] = 4; // MAX_SEATS_BOAT2
        printf("kasjer: Inicjalizuję pamięć współdzieloną dla pojemności statków.\n");
        logKasjer("kasjer: Inicjalizuję pamięć współdzieloną dla pojemności statków.\n");
    }

    if(sharedCap == NULL){
        sharedCap = (int*)shmat(shmidCap, NULL, 0);
        if(sharedCap == (int*)-1){
            perror("kasjer: shmat capacity");
            finish(0);
        }
    }

    logKasjer("Kasjer uruchomiony. Oczekuję na pasażerów i komunikaty.\n");

    while(1){
        struct msgbuf m;
        memset(&m, 0, sizeof(m));
        if(msgrcv(msqid, &m, sizeof(m.mtext), 0, 0) == -1){
            if(errno == EINTR) continue;
            perror("kasjer: msgrcv");
            break;
        }
        char bufTime[TIME_SIZE];
        getTime(bufTime);

        if(m.mtext[0] == '1'){
            int pid, age;
            if(sscanf(m.mtext, "1 %d %d", &pid, &age) != 2) continue;
            float cost = ticketPrice(age);
            revenue += cost;

            // Odczytaj aktualne wolne miejsca
            int freeBoat1 = sharedCap[0];
            int freeBoat2 = sharedCap[1];
            int boat = 1;
            if(age < 15 || age > 70){
                boat = 2;
            }

            if(boat == 1 && freeBoat1 <= 0){
                if(freeBoat2 > 0){
                    boat = 2;
                }
                else{
                    char tmp[256];
                    snprintf(tmp, sizeof(tmp), "[%s] Kasjer: Brak miejsc dla pasażera PID:%d.\n", bufTime, pid);
                    logKasjer(tmp);
                    // Odesłanie pasażera z powrotem (może wysłać specjalny komunikat lub sygnał)
                    continue;
                }
            }
            else if(boat == 2 && freeBoat2 <= 0){
                if(freeBoat1 > 0 && age >= 15 && age <= 70){
                    boat = 1;
                }
                else{
                    char tmp[256];
                    snprintf(tmp, sizeof(tmp), "[%s] Kasjer: Brak miejsc dla pasażera PID:%d.\n", bufTime, pid);
                    logKasjer(tmp);
                    // Odesłanie pasażera z powrotem
                    continue;
                }
            }

            // Przydzielenie miejsca
            if(boat == 1){
                sharedCap[0] = freeBoat1 - 1;
            }
            else{
                sharedCap[1] = freeBoat2 - 1;
            }

            // Przygotowanie komunikatu w zdefiniowanym formacie: pid:boat_num:free_seats
            char out[128];
            snprintf(out, sizeof(out), "%d:%d:%d", pid, boat, (boat == 1 ? sharedCap[0] : sharedCap[1]));
            sendFifo(out, (boat == 1) ? FIFO_STATEK1 : FIFO_STATEK2);

            // Logowanie do kasjerraport.txt i konsoli
            char log_msg[256];
            snprintf(log_msg, sizeof(log_msg),
                "[%s] Kasjer: Pasażer PID: %d, wiek: %d, zapłacił: %.2f i został wysłany na statek: %d. Wolne miejsca na statku: %d.\n",
                bufTime, pid, age, cost, boat, (boat == 1 ? sharedCap[0] : sharedCap[1]));
            logKasjer(log_msg);

            // Wysyłanie do pomostu i logu
            sendFifo(log_msg, FIFO_POMOST);
            sendFifo(log_msg, FIFO_LOG);
        }
        else if(m.mtext[0] == '2'){
            int p1, p2, w1, w2;
            if(sscanf(m.mtext, "2 %d %d %d %d", &p1, &p2, &w1, &w2) != 4) continue;
            float cost = ticketPrice(w1) + ticketPrice(w2);
            revenue += cost;

            // Sprawdzenie miejsc na statek2 (potrzebne 2 miejsca)
            int freeBoat2 = sharedCap[1];
            if(freeBoat2 < 2){
                char tmp[256];
                snprintf(tmp, sizeof(tmp),
                    "[%s] Kasjer: Brak miejsc dla pasażerów PID:%d i PID:%d.\n",
                    bufTime, p1, p2);
                logKasjer(tmp);
                // Odesłanie pasażerów z powrotem
                continue;
            }

            // Przydzielenie miejsc
            sharedCap[1] = freeBoat2 - 2;

            // Przygotowanie komunikatu w zdefiniowanym formacie: p1:boat_num:free_seats;p2:boat_num:free_seats
            char out1[128];
            snprintf(out1, sizeof(out1), "%d:%d:%d", p1, 2, sharedCap[1]);
            sendFifo(out1, FIFO_STATEK2);

            char out2[128];
            snprintf(out2, sizeof(out2), "%d:%d:%d", p2, 2, sharedCap[1]);
            sendFifo(out2, FIFO_STATEK2);

            // Logowanie do kasjerraport.txt i konsoli
            char log_msg[256];
            snprintf(log_msg, sizeof(log_msg),
                "[%s] Kasjer: Pasażerowie PID:%d i PID:%d, zapłacili: %.2f i zostali wysłani na statek:2. Wolne miejsca na statku: %d.\n",
                bufTime, p1, p2, cost, sharedCap[1]);
            logKasjer(log_msg);

            // Wysyłanie do pomostu i logu
            sendFifo(log_msg, FIFO_POMOST);
            sendFifo(log_msg, FIFO_LOG);
        }
        else if(m.mtext[0] == '9'){
            int pid, age;
            if(sscanf(m.mtext, "9 %d %d", &pid, &age) != 2) continue;
            float cost = ticketPrice(age) * 0.5f;
            revenue += cost;

            // Preferencja dla statek1
            int boat = 1;
            int freeBoat1 = sharedCap[0];
            int freeBoat2 = sharedCap[1];
            if(freeBoat1 <= 0){
                if(freeBoat2 > 0){
                    boat = 2;
                }
                else{
                    char tmp[256];
                    snprintf(tmp, sizeof(tmp),
                        "[%s] Kasjer: Brak miejsc (2 rejs) dla PID:%d.\n",
                        bufTime, pid);
                    logKasjer(tmp);
                    // Odesłanie pasażera z powrotem
                    continue;
                }
            }

            // Przydzielenie miejsca
            if(boat == 1){
                sharedCap[0] = freeBoat1 - 1;
            }
            else{
                sharedCap[1] = freeBoat2 - 1;
            }

            // Przygotowanie komunikatu w zdefiniowanym formacie: pid:boat_num:free_seats
            char out[128];
            snprintf(out, sizeof(out), "%d:%d:%d", pid, boat, (boat == 1 ? sharedCap[0] : sharedCap[1]));
            sendFifo(out, (boat == 1) ? FIFO_STATEK1 : FIFO_STATEK2);

            // Logowanie do kasjerraport.txt i konsoli
            char log_msg[256];
            snprintf(log_msg, sizeof(log_msg),
                "[%s] Kasjer: Pasażer (2 rejs) PID:%d, wiek: %d, zapłacił: %.2f, statek: %d, wolne miejsca: %d.\n",
                bufTime, pid, age, cost, boat, (boat == 1 ? sharedCap[0] : sharedCap[1]));
            logKasjer(log_msg);

            // Wysyłanie do pomostu i logu
            sendFifo(log_msg, FIFO_POMOST);
            sendFifo(log_msg, FIFO_LOG);
        }
        else{
            char tmp[128];
            snprintf(tmp, sizeof(tmp),
                "[%s] Kasjer: Nieznany format komunikatu: %s\n",
                bufTime, m.mtext);
            logKasjer(tmp);
        }
    }

    finish(0);
    return 0;
}
