#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

void mymkdir(char *path) {
    if (mkdirat(AT_FDCWD, path, 0) < 0)
        printf("Unable to mkdir %s\n", path);
}

int main(int argc, char *argv[]) {
    /* TODO: Lab9 Shell */
    for (int i = 1; i < argc; i++)
        mymkdir(argv[i]);
    return 0;
}