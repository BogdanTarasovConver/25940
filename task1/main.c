#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <ulimit.h>
#include <string.h>
#include <errno.h>

extern char **environ;

typedef struct {
    char opt;
    char *arg;
} Option;


/* Перевод строки в неотрицательное число */
int get_number(char *str, long *result)
{
    char *end;

    errno = 0;
    *result = strtol(str, &end, 10);

    if (errno != 0 || end == str || *end != '\0' || *result < 0)
        return -1;

    return 0;
}


int main(int argc, char *argv[])
{
    Option options[256];
    int count = 0;
    int opt;
    int i;
    long value;

    opterr = 0;

    /* Сначала только запоминаем опции */
    while ((opt = getopt(argc, argv, ":ispuU:cC:dvV:")) != -1) {

        if (opt == '?') {
            fprintf(stderr, "Unknown option: -%c\n", optopt);
            return 1;
        }

        if (opt == ':') {
            fprintf(stderr, "Option -%c needs an argument\n", optopt);
            return 1;
        }

        if (count >= 256) {
            fprintf(stderr, "Too many options\n");
            return 1;
        }

        options[count].opt = opt;
        options[count].arg = optarg;
        count++;
    }


    /* Теперь выполняем СПРАВА НАЛЕВО */
    for (i = count - 1; i >= 0; i--) {

        switch (options[i].opt) {

        /* реальные и эффективные UID/GID */
        case 'i':
            printf("uid=%ld euid=%ld gid=%ld egid=%ld\n",
                   (long)getuid(),
                   (long)geteuid(),
                   (long)getgid(),
                   (long)getegid());
            break;


        /* стать лидером группы процессов */
        case 's':
            if (setpgid(0, 0) == -1)
                perror("setpgid");
            break;


        /* PID, PPID, process group */
        case 'p':
            printf("pid=%ld ppid=%ld pgrp=%ld\n",
                   (long)getpid(),
                   (long)getppid(),
                   (long)getpgrp());
            break;


        /* показать ulimit */
        case 'u':
            errno = 0;
            value = ulimit(UL_GETFSIZE);

            if (value == -1 && errno != 0)
                perror("ulimit");
            else
                printf("ulimit=%ld\n", value);

            break;


        /* изменить ulimit */
        case 'U':
            if (get_number(options[i].arg, &value) == -1) {
                fprintf(stderr, "Bad U value: %s\n",
                        options[i].arg);
                break;
            }

            errno = 0;

            if (ulimit(UL_SETFSIZE, value) == -1 &&
                errno != 0)
                perror("ulimit");

            break;


        /* показать core limit */
        case 'c': {
            struct rlimit limit;

            if (getrlimit(RLIMIT_CORE, &limit) == -1) {
                perror("getrlimit");
            } else if (limit.rlim_cur == RLIM_INFINITY) {
                printf("core=unlimited\n");
            } else {
                printf("core=%llu bytes\n",
                       (unsigned long long)limit.rlim_cur);
            }

            break;
        }


        /* изменить core limit */
        case 'C': {
            struct rlimit limit;

            if (get_number(options[i].arg, &value) == -1) {
                fprintf(stderr, "Bad C value: %s\n",
                        options[i].arg);
                break;
            }

            if (getrlimit(RLIMIT_CORE, &limit) == -1) {
                perror("getrlimit");
                break;
            }

            limit.rlim_cur = (rlim_t)value;

            if (setrlimit(RLIMIT_CORE, &limit) == -1)
                perror("setrlimit");

            break;
        }


        /* текущая директория */
        case 'd': {
            char path[4096];

            if (getcwd(path, sizeof(path)) == NULL)
                perror("getcwd");
            else
                printf("%s\n", path);

            break;
        }


        /* вывести environment */
        case 'v': {
            char **env;

            for (env = environ; *env != NULL; env++)
                printf("%s\n", *env);

            break;
        }


        /* изменить/добавить переменную environment */
        case 'V':
            if (strchr(options[i].arg, '=') == NULL) {
                fprintf(stderr, "Use: -Vname=value\n");
            } else if (putenv(options[i].arg) != 0) {
                perror("putenv");
            }

            break;
        }
    }

    return 0;
}
