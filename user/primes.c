#include "kernel/types.h"
#include "user/user.h"

void child() {
    int p[2];
    if (pipe(p) < 0) exit(1);

    int pid = fork();
    if (pid == 0) {
        close(0);
        dup(p[0]);
        close(p[0]);
        close(p[1]);
        child();
    } else {
        close(p[0]);

        int prime, num;
        if (read(0, &prime, sizeof(int)) == 0) exit(0);
        fprintf(1, "prime %d\n", prime);

        while (read(0, &num, sizeof(int)) > 0) {
            if (num % prime != 0) {
                write(p[1], &num, sizeof(int));
            }
        }

        close(p[1]);
        wait(0);
    }
}

int main() {
    int p[2];
    if (pipe(p) < 0) exit(1);

    int pid = fork();
    if (pid == 0) {
        close(0);
        dup(p[0]);
        close(p[0]);
        close(p[1]);
        child();
    } else {
        close(p[0]);
        for (int i = 2; i <= 35; i++) {
            write(p[1], &i, 4);
        }
        close(p[1]);
        wait(0);
    }

    exit(0);
}