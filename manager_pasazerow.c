#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

int main(void) {
    int status;
    pid_t pid;
    
    while (1) {
        while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
            /* Opcjonalnie: można wypisywać, że proces został zreapowany:
            printf("Manager: zreapowany proces PID %d\n", pid);
            */
        }
        sleep(1);
    }
    return 0;
}
