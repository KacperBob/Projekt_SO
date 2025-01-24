#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/sem.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <errno.h>

#define SHM_KEY_TIME 5678
#define TIME_SIZE 20

// Główne parametry (można dostosować wg potrzeb projektu)
#define MAX_SEATS_BOAT1 4
#define MAX_SEATS_BOAT2 4
#define TRIP_DURATION   5
#define TIME_LIMIT     20

// Fifo, przez które pomost odbiera informacje np. od kasjera
#define FIFO_BOAT1     "fifo_boat1"
#define FIFO_BOAT2     "fifo_boat2"
#define FIFO_POMOST    "fifo_pomost"

struct pomost_state {
    int passengers_on_bridge; 
    int direction; // 1 – wchodzenie na statek, -1 – schodzenie ze statku, 0 – blokada
};

int shmid_time=-1;
char *shared_time=NULL;

int passengers_boat1=0, passengers_boat2=0;
time_t last_passenger_time_boat1, last_passenger_time_boat2;
struct pomost_state pomost1, pomost2;

void cleanup(int signum){
    if(shared_time!=NULL) shmdt(shared_time);
    exit(0);
}

void pobierzCzas(char*b){
    int id=shmget(SHM_KEY_TIME,TIME_SIZE,0666);
    if(id<0){ snprintf(b,TIME_SIZE,"??:??:??"); return; }
    char*c=(char*)shmat(id,NULL,0);
    if(c==(char*)-1){ snprintf(b,TIME_SIZE,"??:??:??"); return; }
    snprintf(b,TIME_SIZE,"%s",c);
    shmdt(c);
}

void logPomost(const char*s){
    printf("%s",s);
    fflush(stdout);
}

void odczytZfifo(const char*fifo, int *passengers_on_boat, struct pomost_state *pom, int max_seats, time_t *last_pass_time, const char*whichBoat){
    char buf[256];
    int fd=open(fifo,O_RDONLY|O_NONBLOCK);
    if(fd<0){
        // Nie ma fifo albo nie da się otworzyć
        return;
    }
    int r=0;
    while((r=read(fd,buf,sizeof(buf)))>0){
        // Buf może zawierać np. "[06:02:00] Kasjer: Pasazer PID:12345 wiek:30..."
        // lub inny komunikat. My tylko sygnalizujemy, że ktoś wchodzi
        // Odczyt jest 'luzny', bo faktyczna logika dopuszczenia pasażera
        // jest uproszczona: pom->direction=1 => boarding
        buf[r]='\0';
        char czas[TIME_SIZE]; pobierzCzas(czas);

        if(pom->direction==1){
            if(*passengers_on_boat<max_seats){
                (*passengers_on_boat)++;
                *last_pass_time=time(NULL);
                char out[512];
                snprintf(out,sizeof(out),"[%s] Pomost: %s – pasażer wchodzi na statek. Teraz jest %d/%d.\n",
                         czas, whichBoat,*passengers_on_boat,max_seats);
                logPomost(out);
            } else {
                char out[512];
                snprintf(out,sizeof(out),"[%s] Pomost: %s pełny. Pasażer musi czekać.\n",
                         czas, whichBoat);
                logPomost(out);
            }
        } else if(pom->direction==-1){
            // Schodzenie ze statku
            char out[512];
            snprintf(out,sizeof(out),"[%s] Pomost: %s – pasażer opuszcza statek.\n",
                     czas, whichBoat);
            logPomost(out);
            if(*passengers_on_boat>0){
                (*passengers_on_boat)--;
            }
        } else {
            // direction=0 => pomost zablokowany
            char out[512];
            snprintf(out,sizeof(out),"[%s] Pomost: %s – POMOST ZABLOKOWANY, pasażer nie przejdzie.\n",
                     czas, whichBoat);
            logPomost(out);
        }
    }
    close(fd);
}

void startTrip(struct pomost_state *pom, int *passengers_on_boat, const char*whichBoat){
    char czas[TIME_SIZE];pobierzCzas(czas);
    // Blokada pomostu
    pom->direction=0;
    char out[256];
    snprintf(out,sizeof(out),"[%s] Pomost: Rozpoczynam rejs dla %s z %d pasażerami.\n",czas,whichBoat,*passengers_on_boat);
    logPomost(out);

    // Czas rejsu
    sleep(TRIP_DURATION);

    pobierzCzas(czas);
    snprintf(out,sizeof(out),"[%s] Pomost: Rejs %s zakończony. Ustawiam pomost na -1 (schodzenie).\n", czas, whichBoat);
    logPomost(out);

    // Zezwalamy na schodzenie
    pom->direction=-1;
    // w realnej sytuacji pasażerowie wychodzą, co przechwyci odczytZfifo(...),
    // tutaj możemy sztucznie wyzerować *passengers_on_boat = 0 w pętli
    // lub czekać, aż "z czyjegoś side" nadejdzie info o opuszczaniu.
    // Dla uproszczenia:
    sleep(2);
    *passengers_on_boat=0;

    pobierzCzas(czas);
    snprintf(out,sizeof(out),"[%s] Pomost: Wszyscy zeszli, pomost wraca do 1 (boarding).\n", czas);
    logPomost(out);

    // Ponownie otwieramy do wchodzenia
    pom->direction=1;
}

int main(){
    signal(SIGINT,cleanup);
    signal(SIGTERM,cleanup);

    pomost1.passengers_on_bridge=0; 
    pomost1.direction=1; // wchodzenie
    pomost2.passengers_on_bridge=0; 
    pomost2.direction=1;

    passengers_boat1=0;
    passengers_boat2=0;
    last_passenger_time_boat1=time(NULL);
    last_passenger_time_boat2=time(NULL);

    printf("Pomost uruchomiony. Obsługujemy boat1 i boat2.\n");

    while(1){
        odczytZfifo(FIFO_BOAT1,&passengers_boat1,&pomost1,MAX_SEATS_BOAT1,&last_passenger_time_boat1,"Boat1");
        odczytZfifo(FIFO_BOAT2,&passengers_boat2,&pomost2,MAX_SEATS_BOAT2,&last_passenger_time_boat2,"Boat2");

        // Sprawdzamy, czy warto wystartować rejs (np. zapełniony statek lub upłynął TIME_LIMIT)
        if(passengers_boat1>=MAX_SEATS_BOAT1 || difftime(time(NULL),last_passenger_time_boat1)>=TIME_LIMIT){
            if(passengers_boat1>0 && pomost1.direction==1){
                startTrip(&pomost1,&passengers_boat1,"Boat1");
            }
        }
        if(passengers_boat2>=MAX_SEATS_BOAT2 || difftime(time(NULL),last_passenger_time_boat2)>=TIME_LIMIT){
            if(passengers_boat2>0 && pomost2.direction==1){
                startTrip(&pomost2,&passengers_boat2,"Boat2");
            }
        }
        sleep(1);
    }

    cleanup(0);
    return 0;
}
