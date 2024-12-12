#include <stdio.h>
#include <time.h>
#include <unistd.h>

#define MNOZNIK_CZASU 60

void przeskalowany_czas() {
    struct timespec teraz;
    clock_gettime(CLOCK_REALTIME, &teraz);

    time_t czas_w_sekundach = teraz.tv_sec;
    time_t czas_symulowany = czas_w_sekundach * MNOZNIK_CZASU;

    struct tm *czas_info = gmtime(&czas_symulowany);
    printf("Czas symulowany: %02d:%02d:%02d\n", 
           czas_info->tm_hour, 
           czas_info->tm_min, 
           czas_info->tm_sec);
}

int main() {
    while (1) {
        przeskalowany_czas();
        sleep(1);
    }
    return 0;
}

