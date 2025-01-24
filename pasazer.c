#include <fcntl.h>     // <-- DODAJ TO
#include <sys/stat.h> 
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/wait.h>

#define MSG_KEY 1234
#define SHM_KEY 5678
#define TIME_SIZE 20

struct msgbuf {
    long mtype;
    char mtext[128];
};

static volatile sig_atomic_t running = 1;

void sig_handler(int s){
    running = 0;
}

void zapisRaport(const char*s){
    int f = open("pasazerraport.txt", O_WRONLY|O_CREAT|O_APPEND, 0666);
    if(f>=0){
        write(f, s, strlen(s));
        close(f);
    }
}

void pobierzCzas(char*b){
    int id = shmget(SHM_KEY, TIME_SIZE, 0666);
    if(id<0){
        snprintf(b, TIME_SIZE, "??:??:??");
        return;
    }
    char*c = (char*)shmat(id, NULL, 0);
    if(c==(char*)-1){
        snprintf(b, TIME_SIZE, "??:??:??");
        return;
    }
    snprintf(b, TIME_SIZE, "%s", c);
    shmdt(c);
}

int main(){
    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);

    srand(getpid() * time(NULL));

    int msqid = msgget(MSG_KEY, 0666);
    if(msqid < 0){
        printf("Brak kolejki komunikatów MSG_KEY=%d\n", MSG_KEY);
        return 0;
    }

    char czas[TIME_SIZE];
    int baseAge = (rand()%60)+15;
    int hasDependent = (rand()%100)<30;
    pid_t child=0;

    pobierzCzas(czas);
    if(!hasDependent){
        printf("[%s] Pasażer: PID:%d, wiek:%d. Pojawił się.\n", czas, getpid(), baseAge);
        {
            char tmp[128];
            snprintf(tmp,sizeof(tmp),"[%s] Pasażer: PID:%d, wiek:%d. Pojawił się.\n", czas, getpid(), baseAge);
            zapisRaport(tmp);
        }
    } else {
        child = fork();
        if(child<0){
            perror("fork");
            return 1;
        }
        if(child==0){
            char czas2[TIME_SIZE];
            int depAge = ((rand()%2)==0)?(rand()%15):((rand()%26)+75);
            pobierzCzas(czas2);
            printf("[%s] Pasażer potomny: PID:%d, PPID:%d, wiek:%d. Pojawił się.\n", czas2, getpid(), getppid(), depAge);
            {
                char buf[128];
                snprintf(buf,sizeof(buf),"[%s] Pasażer potomny: PID:%d, PPID:%d, wiek:%d. Pojawił się.\n",
                         czas2, getpid(), getppid(), depAge);
                zapisRaport(buf);
            }
            struct msgbuf m2;
            memset(&m2,0,sizeof(m2));
            m2.mtype=2;
            snprintf(m2.mtext,sizeof(m2.mtext),"2 %d %d %d %d",(int)getppid(),(int)getpid(),(int)baseAge,depAge);
            msgsnd(msqid,&m2,strlen(m2.mtext)+1,0);

            // Czekamy na "zejście ze statku" – tu wystarczy pętla czekania na SIGTERM lub SIGINT
            while(running){
                sleep(1);
            }
            pobierzCzas(czas2);
            printf("[%s] Pasażer potomny: PID:%d, schodzę ze statku i kończę.\n", czas2, getpid());
            {
                char buf[128];
                snprintf(buf,sizeof(buf),"[%s] Pasażer potomny: PID:%d. Schodzę ze statku i kończę.\n", czas2, getpid());
                zapisRaport(buf);
            }
            return 0;
        } else {
            printf("[%s] Pasażerowie: PID(rodzic):%d, PID(potomny):%d, wiek1:%d. Pojawili się.\n",
                   czas, getpid(), child, baseAge);
            {
                char tmp[128];
                snprintf(tmp,sizeof(tmp),"[%s] Pasażerowie: PID(rodzic):%d, PID(potomny):%d, wiek1:%d. Pojawili się.\n",
                         czas, getpid(), child, baseAge);
                zapisRaport(tmp);
            }
        }
    }

    if(!hasDependent){
        struct msgbuf m;
        memset(&m,0,sizeof(m));
        m.mtype=1;
        snprintf(m.mtext,sizeof(m.mtext),"1 %d %d",(int)getpid(),baseAge);
        msgsnd(msqid,&m,strlen(m.mtext)+1,0);
    } else {
        // Wysyłamy informację o rodzicu – bo potomek już wysłał swoją w m2
        struct msgbuf m;
        memset(&m,0,sizeof(m));
        m.mtype=1;
        snprintf(m.mtext,sizeof(m.mtext),"1 %d %d",(int)getpid(),baseAge);
        msgsnd(msqid,&m,strlen(m.mtext)+1,0);
    }

    // Symulacja czekania na wyjście ze statku
    // Może też chcieć drugi rejs
    sleep((rand()%4)+2);
    int secondRide = (rand()%100)<5;
    if(secondRide){
        pobierzCzas(czas);
        printf("[%s] Pasażer: PID:%d, wiek:%d. Decyduję się na drugi rejs (ulga 50%%).\n", czas,getpid(),baseAge);
        {
            char tmp[128];
            snprintf(tmp,sizeof(tmp),"[%s] Pasażer: PID:%d, wiek:%d. Drugi rejs (ulga 50%%).\n", czas,getpid(),baseAge);
            zapisRaport(tmp);
        }
        struct msgbuf m2;
        memset(&m2,0,sizeof(m2));
        m2.mtype=9;
        snprintf(m2.mtext,sizeof(m2.mtext),"9 %d %d",(int)getpid(),baseAge);
        msgsnd(msqid,&m2,strlen(m2.mtext)+1,0);
    }

    while(running){
        sleep(1);
    }
    pobierzCzas(czas);
    if(!hasDependent){
        printf("[%s] Pasażer: PID:%d, schodzę ze statku i kończę.\n", czas,getpid());
        {
            char tmp[128];
            snprintf(tmp,sizeof(tmp),"[%s] Pasażer: PID:%d, schodzę ze statku i kończę.\n", czas,getpid());
            zapisRaport(tmp);
        }
    } else {
        wait(NULL);
        printf("[%s] Pasażer rodzic: PID:%d, zszedłem po potomnym i kończę.\n", czas,getpid());
        {
            char tmp[128];
            snprintf(tmp,sizeof(tmp),"[%s] Pasażer rodzic: PID:%d. Zszedł po potomnym i kończy.\n", czas,getpid());
            zapisRaport(tmp);
        }
    }
    return 0;
}
