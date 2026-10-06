#define _POSIX_C_SOURCE 200809L

#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <inttypes.h>

static void check_file(const char *path)
{
    FILE *file;

    printf("UID=%ju EUID=%ju\n",
           (uintmax_t)getuid(), (uintmax_t)geteuid());
    fflush(stdout);

    file = fopen(path, "r+");

    if (!file) {
        perror(path);
        return;
    }

    puts("File opened successfully");

    if (fclose(file) == EOF)
        perror("fclose");
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s data-file\n", argv[0]);
        return 1;
    }

    puts("Before setuid:");
    check_file(argv[1]);

    if (setuid(getuid()) < 0) {
        perror("setuid");
        return 1;
    }

    puts("After setuid:");
    check_file(argv[1]);

    return 0;
}
