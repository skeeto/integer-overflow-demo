#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>

struct mstack {
    char *top;
    char *max;
    char buf[];
};

struct mstack *
mstack_init(void *p, size_t size)
{
    struct mstack *m = p;
    m->max = (char *)p + size;
    m->top = m->buf;
    return m;
}

void *
mstack_alloc(struct mstack *m, size_t size)
{
    void *p = 0;
    size_t avail = m->max - m->top;
    if (avail > size) {
        p = m->top;
        m->top += size;
    }
    return p;
}

void
mstack_free(struct mstack *m, void *p)
{
    m->top = p;
}

int
authenticate(struct mstack *m)
{
    FILE *tty = fopen("/dev/tty", "r+");
    if (!tty) {
        perror("/dev/tty");
        return 0;
    }

    char *user = mstack_alloc(m, 32);
    if (!user) {
        fclose(tty);
        return 0;
    }
    fputs("User: ", tty);
    fflush(tty);
    if (!fgets(user, 32, tty))
        user[0] = 0;

    char *pass = mstack_alloc(m, 32);
    int result = 0;
    if (pass) {
        fputs("Password: ", tty);
        fflush(tty);
        if (fgets(pass, 32, tty))
            result = strcmp(user, pass) != 0;
    }

    fclose(tty);
    mstack_free(m, user);
    return result;
}

void *
naive_calloc(struct mstack *m, unsigned long nmemb, unsigned long size)
{
    void *p = mstack_alloc(m, nmemb * size);
    if (p)
        memset(p, 0, nmemb * size);
    return p;
}

unsigned long
safe_strtoul(char *nptr, char **endptr, int base)
{
    errno = 0;
    unsigned long n = strtoul(nptr, endptr, base);
    if (errno) {
        perror(nptr);
        exit(EXIT_FAILURE);
    } else if (nptr == *endptr) {
        fprintf(stderr, "Expected an integer\n");
        exit(EXIT_FAILURE);
    } else if (!isspace(**endptr)) {
        fprintf(stderr, "Invalid character '%c'\n", **endptr);
        exit(EXIT_FAILURE);
    }
    return n;
}

int
main(void)
{
    static char membuf[16L * 1024 * 1024];
    struct mstack *m = mstack_init(membuf, sizeof(membuf));

    if (!authenticate(m)) {
        fputs("Authentication failure\n", stderr);
        exit(EXIT_FAILURE);
    }

    /* Read image header. */
    char line[256];
    char *p = line;
    if (!fgets(line, sizeof(line), stdin)) {
        fputs("Header read error\n", stderr);
        exit(EXIT_FAILURE);
    }
    if (strncmp(p, "V2 ", 3) != 0) {
        fputs("Bad header magic\n", stderr);
        exit(EXIT_FAILURE);
    }
    p += 3;

    /* Allocate image data based on header. */
    unsigned long width = safe_strtoul(p, &p, 10);
    unsigned long height = safe_strtoul(p, &p, 10);
    unsigned char *pixels = naive_calloc(m, width, height);
    if (!pixels) {
        fputs("Not enough memory\n", stderr);
        exit(EXIT_FAILURE);
    }

    /* Process command stream. */
    while (fgets(line, sizeof(line), stdin)) {
        unsigned long x, y, v;
        char *p = line;
        switch (*p) {
            case 's':
                x = safe_strtoul(p + 1, &p, 10);
                y = safe_strtoul(p, &p, 10);
                v = safe_strtoul(p, &p, 16);
                if (x < width && y < height)
                    pixels[y * width + x] = v;
                break;
            default:
                fprintf(stderr, "Bad command: %s", line);
                exit(EXIT_FAILURE);
        }
    }

    /* Write rendered image. */
    printf("P2\n%ld %ld 255\n", width, height);
    for (unsigned long y = 0; y < height; y++) {
        for (unsigned long x = 0; x < width; x++)
            printf("%d ", pixels[y * width + x]);
        putchar('\n');
    }
}
