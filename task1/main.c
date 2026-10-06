#define _POSIX_C_SOURCE 200809L

#include <sys/types.h>
#include <sys/resource.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <errno.h>
#include <ctype.h>

extern char **environ;

struct option {
    int name;
    char *argument;
    struct option *next;
};

static void usage(void)
{
    fprintf(stderr,
        "Options: -i -s -p -u -U number -c -C bytes "
        "-d -v -V name=value\n"
        "Execution order: right to left.\n");
}

static void print_limit(rlim_t value, uintmax_t unit)
{
    if (value == RLIM_INFINITY)
        puts("unlimited");
#ifdef RLIM_SAVED_CUR
    else if (value == RLIM_SAVED_CUR)
        puts("saved-current");
#endif
#ifdef RLIM_SAVED_MAX
    else if (value == RLIM_SAVED_MAX)
        puts("saved-maximum");
#endif
    else
        printf("%ju\n", (uintmax_t)value / unit);
}

static int parse_number(const char *text, uintmax_t *value)
{
    char *end;

    if (!text || !isdigit((unsigned char)text[0])) {
        fprintf(stderr, "Expected a nonnegative integer\n");
        return -1;
    }

    errno = 0;
    *value = strtoumax(text, &end, 10);

    if (errno || *end) {
        fprintf(stderr, "Invalid number: %s\n", text);
        return -1;
    }

    return 0;
}

static int change_limit(int resource, const char *text,
                        uintmax_t unit)
{
    struct rlimit limit;
    uintmax_t value;
    rlim_t converted;

    if (parse_number(text, &value) < 0)
        return -1;

    if (value > UINTMAX_MAX / unit)
        goto invalid;

    value *= unit;
    converted = (rlim_t)value;

    if ((uintmax_t)converted != value ||
        converted == RLIM_INFINITY)
        goto invalid;

#ifdef RLIM_SAVED_CUR
    if (converted == RLIM_SAVED_CUR)
        goto invalid;
#endif
#ifdef RLIM_SAVED_MAX
    if (converted == RLIM_SAVED_MAX)
        goto invalid;
#endif

    if (getrlimit(resource, &limit) < 0) {
        perror("getrlimit");
        return -1;
    }

    limit.rlim_cur = converted;

    if (setrlimit(resource, &limit) < 0) {
        perror("setrlimit");
        return -1;
    }

    return 0;

invalid:
    fprintf(stderr, "Limit is too large\n");
    return -1;
}

static int show_u(void)
{
#if defined(FILE_LIMIT)
    struct rlimit limit;

    if (getrlimit(RLIMIT_FSIZE, &limit) < 0) {
        perror("getrlimit");
        return -1;
    }

    print_limit(limit.rlim_cur, 512);

#elif defined(RLIMIT_NPROC)
    struct rlimit limit;

    if (getrlimit(RLIMIT_NPROC, &limit) < 0) {
        perror("getrlimit");
        return -1;
    }

    print_limit(limit.rlim_cur, 1);

#else
    long value;

    errno = 0;
    value = sysconf(_SC_CHILD_MAX);

    if (value == -1 && errno) {
        perror("sysconf");
        return -1;
    }

    if (value == -1)
        puts("unlimited");
    else
        printf("%ld\n", value);
#endif

    return 0;
}

static int change_u(const char *text)
{
#if defined(FILE_LIMIT)
    return change_limit(RLIMIT_FSIZE, text, 512);
#elif defined(RLIMIT_NPROC)
    return change_limit(RLIMIT_NPROC, text, 1);
#else
    uintmax_t value;

    if (parse_number(text, &value) < 0)
        return -1;

    fprintf(stderr,
        "-U: native illumos has no RLIMIT_NPROC; "
        "CHILD_MAX cannot be changed through sysconf.\n");
    return -1;
#endif
}

static int execute(struct option *option)
{
    struct rlimit limit;
    char path[4096];
    char **env;
    char *assignment, *equal;

    switch (option->name) {
    case 'i':
        printf("UID=%ju EUID=%ju GID=%ju EGID=%ju\n",
               (uintmax_t)getuid(), (uintmax_t)geteuid(),
               (uintmax_t)getgid(), (uintmax_t)getegid());
        break;

    case 's':
        if (setpgid(0, 0) < 0) {
            perror("setpgid");
            return -1;
        }
        break;

    case 'p':
        printf("PID=%jd PPID=%jd PGID=%jd\n",
               (intmax_t)getpid(), (intmax_t)getppid(),
               (intmax_t)getpgrp());
        break;

    case 'u':
        return show_u();

    case 'U':
        return change_u(option->argument);

    case 'c':
        if (getrlimit(RLIMIT_CORE, &limit) < 0) {
            perror("getrlimit");
            return -1;
        }
        print_limit(limit.rlim_cur, 1);
        break;

    case 'C':
        return change_limit(RLIMIT_CORE, option->argument, 1);

    case 'd':
        if (!getcwd(path, sizeof(path))) {
            perror("getcwd");
            return -1;
        }
        puts(path);
        break;

    case 'v':
        for (env = environ; *env; ++env)
            puts(*env);
        break;

    case 'V':
        assignment = strdup(option->argument);
        if (!assignment) {
            perror("strdup");
            return -1;
        }

        equal = strchr(assignment, '=');
        if (!equal || equal == assignment) {
            fprintf(stderr, "-V expects name=value\n");
            free(assignment);
            return -1;
        }

        *equal = '\0';

        if (setenv(assignment, equal + 1, 1) < 0) {
            perror("setenv");
            free(assignment);
            return -1;
        }

        free(assignment);
        break;
    }

    return 0;
}

int main(int argc, char **argv)
{
    struct option *head = NULL, *node;
    int opt, status = 0;

    opterr = 0;

    while ((opt = getopt(argc, argv, ":ispuU:cC:dvV:")) != -1) {
        if (opt == '?' || opt == ':') {
            fprintf(stderr,
                    "Invalid option or missing argument: -%c\n",
                    optopt);
            status = 1;
            goto cleanup;
        }

        node = malloc(sizeof(*node));
        if (!node) {
            perror("malloc");
            status = 1;
            goto cleanup;
        }

        node->name = opt;
        node->argument = optarg;
        node->next = head;
        head = node;
    }

    if (optind != argc) {
        usage();
        status = 1;
        goto cleanup;
    }

    if (!head)
        usage();

    while (head) {
        node = head;
        head = head->next;

        if (execute(node) < 0)
            status = 1;

        free(node);
    }

cleanup:
    while (head) {
        node = head;
        head = head->next;
        free(node);
    }

    if (fflush(stdout) == EOF) {
        perror("stdout");
        status = 1;
    }

    return status;
}
