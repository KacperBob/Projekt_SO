#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

int main(void) {
    int status;
    pid_t pid;
    
    // Nieskończona pętla, która co sekundę reapuje zakończone procesy
    while (1) {
        // waitpid(-1, ...) oznacza, że czekamy na dowolne zakończone dziecko
        while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
            // Opcjonalnie: można wyświetlić informacje o zbitym dziecku
            // printf("Menedżer pasażerów: zbito proces o PID: %d\n", pid);
        }
        sleep(1);  // czekamy sekundę przed kolejną iteracją
    }
    return 0;
}
