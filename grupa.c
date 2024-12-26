#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/types.h>

#define MSG_KEY 1234
#define MAX_WIEK 100
#define MIN_WIEK 1
#define MAX_GRUPA 12

struct osoba {
    int wiek;
    pid_t pid;
};

struct grupa {
    long mtype;
    int rozmiar; // Rozmiar grupy
    struct osoba osoby[MAX_GRUPA];
};

int losuj_rozmiar_grupy() {
    int los = rand() % 100 + 1;
    if (los <= 40) return 1;
    if (los <= 65) return 2;
    if (los <= 80) return 3;
    if (los <= 90) return 4;
    return rand() % (MAX_GRUPA - 4 + 1) + 5;
}

void raportuj_grupa(int rozmiar, struct osoba osoby[]) {
    time_t teraz = time(NULL);
    struct tm *czas_info = localtime(&teraz);
    char czas_str[20];
    strftime(czas_str, sizeof(czas_str), "%H:%M:%S", czas_info);

    FILE *plik = fopen("raport_pasażerów.txt", "a");
    if (plik == NULL) {
        perror("Nie można otworzyć pliku raportu");
        exit(1);
    }

    fprintf(plik, "[%s] Grupa %d osób zgłosiła się do kasjera:\n", czas_str, rozmiar);
    for (int i = 0; i < rozmiar; i++) {
        fprintf(plik, "  - Osoba %d: wiek %d, PID %d\n", i + 1, osoby[i].wiek, osoby[i].pid);
    }
    fclose(plik);

    printf("[%s] Grupa %d osób zgłosiła się do kasjera:\n", czas_str, rozmiar);
    for (int i = 0; i < rozmiar; i++) {
        printf("  - Osoba %d: wiek %d, PID %d\n", i + 1, osoby[i].wiek, osoby[i].pid);
    }
}

int main() {
    srand(getpid());
    int rozmiar = losuj_rozmiar_grupy();

    struct grupa grupa_msg;
    grupa_msg.mtype = 1;
    grupa_msg.rozmiar = rozmiar;

    for (int i = 0; i < rozmiar; i++) {
        grupa_msg.osoby[i].wiek = MIN_WIEK + rand() % (MAX_WIEK - MIN_WIEK + 1);
        grupa_msg.osoby[i].pid = getpid();
    }

    int msgid = msgget(MSG_KEY, 0666);
    if (msgid == -1) {
        perror("Nie można połączyć się z kolejką komunikatów");
        exit(1);
    }

    if (msgsnd(msgid, &grupa_msg, sizeof(grupa_msg) - sizeof(long), 0) == -1) {
        perror("Nie udało się wysłać wiadomości do kasjera");
        exit(1);
    }

    raportuj_grupa(rozmiar, grupa_msg.osoby);

    sleep(rand() % 5 + 1);

    return 0;
}
