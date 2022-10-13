#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/stat.h"


void find(const char *pathFrom, const char *toFind) {
    int fd;
    
    if ((fd = open(pathFrom, 0)) < 0) {
        fprintf(2, "error accessing the specified directory\n");
        exit(1);
    }

    struct stat st;
    if (fstat(fd, &st) < 0) {
        fprintf(2, "cannot stat %s\n", pathFrom);
        close(fd);
        return;
    }

    char buf[512];
    if(strlen(pathFrom) + 1 + DIRSIZ + 1 > sizeof(buf)){
      fprintf(2, "ls: path too long\n");
      close(fd);
      return;
    }
    strcpy(buf, pathFrom);
    
    char *p;
    p = buf + strlen(pathFrom);
    *p++ = '/';

    struct dirent de;
    while (read(fd, &de, sizeof(de)) == sizeof(de)) {
        if (de.inum == 0) continue;

        strcpy(p, de.name);

        if (stat(buf, &st) < 0) {
            fprintf(2, "cannot stat %s\n", buf);
            continue;
        }

        switch (st.type)
        {
        case T_DEVICE:
        case T_FILE:
            if (strcmp(de.name, toFind) == 0)
                fprintf(1, "%s\n", buf);
            break;

        case T_DIR:
            if (strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0)
                continue;
                
            find(buf, toFind);
            break;
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(2, "Usage: find from to_find\n");
        exit(1);
    }
    
    find(argv[1], argv[2]);

    exit(0);
}
