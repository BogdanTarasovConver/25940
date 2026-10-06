#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main(void)
{
    time_t now;
    struct tm *local;
    char text[128];

    if (setenv("TZ", "PST8", 1) < 0) {
        perror("setenv");
        return 1;
    }

    tzset();

    now = time(NULL);
    if (now == (time_t)-1) {
        perror("time");
        return 1;
    }

    local = localtime(&now);
    if (!local) {
        fprintf(stderr, "localtime failed\n");
        return 1;
    }

    if (!strftime(text, sizeof(text),
                  "%Y-%m-%d %H:%M:%S %Z", local)) {
        fprintf(stderr, "strftime failed\n");
        return 1;
    }

    puts(text);
    return 0;
}
