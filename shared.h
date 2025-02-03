#ifndef SHARED_H
#define SHARED_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <string.h>
#include <signal.h>
#include <stdarg.h>
#include <stddef.h>

#define SHM_KEY 0x1234
#define MSGQ_KEY 0x5678
#define SEM_KEY 0x9ABC
#define FIFO_BASE "/tmp/statek"
#define MAX_LOG_SIZE 1024

struct SharedData {
    int Tp;
    int Tk;
    int N1;
    int N2;
    int K;
    int T1;
    int T2;
    time_t sim_time;
    int use_virtual_time;
    int running;
    int boats_current[2];
    int police_blocked[2];
};

struct TicketRequest {
    long mtype;
    int ages[2];
    int group_size;
    int repeat;
    pid_t pid;
};

struct TicketResponse {
    long mtype;
    int allowed;
    int boat;
};

union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

void logger(const char* filename, const char* role, int id, const char* format, ...);

#endif
