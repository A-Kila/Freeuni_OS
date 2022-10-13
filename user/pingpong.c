#include <kernel/types.h>
#include <user/user.h>

int main(int argc, char *argv[]) {
    int p[2];
    if (pipe(p) < 0) exit(1); 

    int pid = fork();
    if (pid == 0) {
        char buff;
        read(p[0], &buff, 1);

        printf("%d: received ping\n", getpid());

        write(p[1], &buff, 1);
    } else {
        char buff = 1;
        write(p[1], &buff, 1);

        read(p[0], &buff, 1);
        printf("%d: received pong\n", getpid());
    }

    close(p[0]);
    close(p[1]);
    exit(0);
}