#include "kernel/types.h"
#include "user/user.h"
#include "kernel/param.h"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(2, "Usage: xargs program [...args]");
        exit(1);
    }

    char *passArgv[MAXARG];
    memcpy(passArgv, argv + 1, sizeof(char *) * (argc - 1));

    char buf[512];
    for (int i = 0, argCount = 0, argStart = 0; read(0, buf + i, 1) != 0; i++) {
        if (i == 511) {
            fprintf(2, "Arguments are too long");
            exit(1);
        }
        
        if (buf[i] == ' ') {
            buf[i] = 0;
            passArgv[argc - 1 + argCount++] = buf + argStart;
            argStart = i + 1;
            continue;
        }

        if (buf[i] != '\n') continue;
 
        buf[i] = 0;
        passArgv[argc - 1 + argCount++] = buf + argStart;
        passArgv[argc - 1 + argCount] = 0;

        int pid = fork();
        if (pid == 0) {
            exec(argv[1], passArgv);

            fprintf(2, "error executing %s\n", argv[1]);
            exit(1);
        }

        wait(0);
        i = 0;
        argCount = 0;
        argStart = 0;
    }

    exit(0);
}