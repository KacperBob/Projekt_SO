#include "shared.h"

void cleanup(int sig) {
    exit(0);
}

int main(int argc, char **argv) {
    signal(SIGINT, cleanup);
    signal(SIGTERM, cleanup);
    
    if(argc != 3) {
        fprintf(stderr, "Użycie: %s <czas> <krok>\n", argv[0]);
        return 1;
    }

    int duration = atoi(argv[1]) * 60;
    int time_step = atoi(argv[2]);

    int shmid = shmget(SHM_KEY, 0, 0);
    struct SharedData *data = (struct SharedData*)shmat(shmid, NULL, 0);
    
    data->sim_time = data->Tp * 3600;
    data->use_virtual_time = 1;
    data->running = 1;

    logger("time.log", "CZAS", 0, "Rozpoczęcie symulacji");
    
    for(int t=0; t<duration && data->running; t+=time_step) {
        data->sim_time += time_step;
        sleep(1);
    }

    data->running = 0;
    shmdt(data);
    return 0;
}
