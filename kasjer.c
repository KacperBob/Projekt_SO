#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/types.h>

#define MSG_KEY 1234
#define MAX_GRUPA 12

struct osoba {
    int wiek;
    pid_t pid;
};

struct grupa {
    long mtype;
    int rozmiar;
    struct osoba osoby[MAX_GRUPA];
};

void raportuj_grupa(int rozmiar, struct osoba osoby[]) {
    time_t teraz = time(NULL);
    struct tm *czas_info = localtime(&teraz);
    char czas_str[20];
    strftime(czas_str, sizeof(czas_str), "%H:%M:%S", czas_info);

    FILE *plik = fopen("raport_kasjera.txt", "a");
    if (plik == NULL) {
        perror("Nie można otworzyć pliku raportu");
        exit(1);
    }

    fprintf(plik, "[%s] Kasjer obsłużył grupę %d osób:\n", czas_str, rozmiar);
    for (int i = 0; i < rozmiar; i++) {
        fprintf(plik, "  - Osoba %d: wiek %d, PID %d\n", i + 1, osoby[i].wiek, osoby[i].pid);
    }
    fclose(plik);

    printf("[%s] Kasjer obsłużył grupę %d osób:\n", czas_str, rozmiar);
    for (int i = 0; i < rozmiar; i++) {
        printf("  - Osoba %d: wiek %d, PID %d\n", i + 1, osoby[i].wiek, osoby[i].pid);
    }
}

int main() {
    int msgid = msgget(MSG_KEY, IPC_CREAT | 0666);
    if (msgid == -1) {
        perror("Nie można utworzyć kolejki komunikatów");
        exit(1);
    }

    struct grupa grupa_msg;
    while (1) {
        if (msgrcv(msgid, &grupa_msg, sizeof(grupa_msg) - sizeof(long), 1, 0) == -1) {
            perror("Błąd odbierania wiadomości");
            continue;
        }

        raportuj_grupa(grupa_msg.rozmiar, grupa_msg.osoby);
    }

    return 0;
}

