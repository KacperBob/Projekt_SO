#include "shared.h"

int main(int argc, char *argv[]) {
    if(argc != 8) {
        fprintf(stderr, "Użycie: %s Tp Tk N1 N2 K T1 T2\n", argv[0]);
        return 1;
    }

    int shmid = shmget(SHM_KEY, sizeof(struct SharedData), IPC_CREAT | 0666);
    if(shmid == -1) {
        perror("shmget failed");
        exit(EXIT_FAILURE);
    }

    struct SharedData *data = (struct SharedData*)shmat(shmid, NULL, 0);
    data->Tp = atoi(argv[1]);
    data->Tk = atoi(argv[2]);
    data->N1 = atoi(argv[3]);
    data->N2 = atoi(argv[4]);
    data->K = atoi(argv[5]);
    data->T1 = atoi(argv[6]);
    data->T2 = atoi(argv[7]);
    data->sim_time = data->Tp * 3600;
    data->use_virtual_time = 0;
    data->running = 1;

    logger("system.log", "SYSTEM", 0, "Zainicjalizowano pamięć współdzieloną");
    shmdt(data);
    return 0;
}
