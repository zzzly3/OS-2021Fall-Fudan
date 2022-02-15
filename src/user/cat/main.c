#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>


void cat(char *path) {
    char buf[513], *p;
    int fd;

    if ((fd = open(path, O_RDONLY)) < 0) {
        fprintf(stderr, "cat: cannot open %s\n", path);
        return;
    }

    while (1)
    {
    	int r = read(fd, &buf, 512);
    	buf[r] = 0;
    	printf("%s", r);
    	if (r < 512)
    		break;
    }
    printf("\n");
    close(fd);
}

int
main(int argc, char *argv[])
{
	/* TODO: Lab9 Shell */
	for (int i = 1; i < argc; i++)
        cat(argv[i]);
    return 0;
}
