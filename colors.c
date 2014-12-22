/* colors.c */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <regex.h>
#include <dlfcn.h>
#include <errno.h>
#include <math.h>

/* Declare a hooked function. */
#define DECLARE_HOOK(name, returns, ...) \
    static returns (*__orig_##name)(__VA_ARGS__) = NULL

/* Initialize a hooked function. */
#define INITIALIZE_HOOK(name) \
    do { \
        *(void **)(&__orig_##name) = dlsym(RTLD_NEXT, #name); \
        if (__orig_##name == NULL) { \
            errno = ENOSYS; \
            abort(); \
        } \
    } while(0)

/* The name of the running program. */
extern char *program_invocation_short_name;

/* A blacklist of programs we can't break, yet. */
static regex_t blacklist_regex;

/* Strip all ANSI color codes. */
static regex_t ansi_escape_regex;

/* Bring in magic, we're going to need it. */
extern unsigned char __magic[];
extern unsigned int __magic_len;

DECLARE_HOOK(write, ssize_t, int, const void *, size_t);

DECLARE_HOOK(__printf_chk, int, int, const char *, ...);
DECLARE_HOOK(printf, int, const char *, ...);

DECLARE_HOOK(fwrite, size_t, const void *, size_t, size_t, FILE *);
DECLARE_HOOK(fwrite_unlocked, size_t, const void *, size_t, size_t, FILE *);

DECLARE_HOOK(__fprintf_chk, int, FILE *, int, const char *, ...);
DECLARE_HOOK(vfprintf, int, FILE *, const char *, va_list);
DECLARE_HOOK(fprintf, int, FILE *, const char *, ...);

DECLARE_HOOK(fputc, int, int, FILE *);
DECLARE_HOOK(fputc_unlocked, int, int, FILE *);

DECLARE_HOOK(fputs, int, const char *, FILE *);
DECLARE_HOOK(fputs_unlocked, int, const char *, FILE *);

DECLARE_HOOK(putchar, int, int);
DECLARE_HOOK(puts, int, const char *);

/* Predicate function for allowing colorization. */
static unsigned int should_colorize(const int fd) {
    const char *bypass = getenv("COLOR_ME_SHOCKED");

    /* Only colorize interactive terminals. */
    if (isatty(fd) == 0)
        return 0;

    /* If the bypass is enabled, skip colorization. */
    if (bypass != NULL && strcmp(bypass, "1") == 0)
        return 0;

    /* We should only ever need to apply this to stderr/stdout. */
    if (fd != STDOUT_FILENO && fd != STDERR_FILENO)
        return 0;

    /* Don't colorize specific blacklisted programs. No guarantees. */
    if (regexec(&blacklist_regex, program_invocation_short_name, 0, NULL, 0) == 0) 
        return 0;

    return 1;
}

/* Set the terminal output color. */
static ssize_t set_terminal_color(const int fd, const double frequency, const double i) {
    unsigned char buffer[16];
    size_t length = 0;

    /* Calculate the wave rotation frequency. */
    double base  = (frequency * i);

    /* Generate three cyclic waves with varying phases. */
    double red   = sin(base + 0 * M_PI / 3) * 127 + 128;
    double green = sin(base + 2 * M_PI / 3) * 127 + 128;
    double blue  = sin(base + 4 * M_PI / 3) * 127 + 128;

    /* Find the red/green/blue indexes into the ANSI colors. */
    unsigned char ri = (unsigned char)(6 * (red   / 256.0));
    unsigned char gi = (unsigned char)(6 * (green / 256.0));
    unsigned char bi = (unsigned char)(6 * (blue  / 256.0));

    /* Derive the ANSI color code from the indexes. */
    unsigned char code = ((36 * ri) + (6 * gi) + bi) + 16;

    /* Print the code into a buffer. */
    length = snprintf(buffer, sizeof buffer, "\x1b[38;5;%hhum", code);

    /* Write the code to the fd, bypassing modifications. */
    return (*__orig_write)(fd, buffer, length);
}

/* Reset the terminal to use normal color. */
static ssize_t reset_terminal_color(const int fd) {
    unsigned const char *reset = "\x1b[0;00m";
    return (*__orig_write)(fd, reset, 7);
}

/* Skip to the next byte after an ANSI escape code. */
static size_t skip_ansi_color_codes(const char *p) {
    regmatch_t match;

    /* Use a regex to find the next byte after an ANSI color escape. */
    if (regexec(&ansi_escape_regex, p, 1, &match, 0) == 0) {
        return match.rm_eo;
    }

    /* No match. */
    return 0;
}

ssize_t write(int fd, const void *buffer, size_t count) {
    unsigned char *p = (unsigned char *) buffer;
    size_t skip = 0, written = 0;

    if (count == 0)
        return 0;

    if (should_colorize(fd)) {
        do {
            set_terminal_color(fd, 0.25, written);
            skip = skip_ansi_color_codes(p + written);
            written += (skip == 0) ? (*__orig_write)(fd, p + written, 1) : skip;
        } while(written < count);

        reset_terminal_color(fd);

        return written;
    }

    return (*__orig_write)(fd, buffer, count);
}

size_t fwrite(const void *data, size_t size, size_t count, FILE *stream) {
    unsigned char *p = (unsigned char *) data;
    const int fd = fileno(stream);
    ssize_t skip = 0, written = 0;

    if ((size * count) == 0)
        return 0;

    if (should_colorize(fd)) {
        do {
            set_terminal_color(fd, 0.25, written);
            skip = skip_ansi_color_codes(p + written);
            written += (skip == 0) ? (*__orig_fwrite)(p + written, 1, 1, stream) : skip;
            fflush(stream);
        } while(written < (size * count));

        reset_terminal_color(fd);

        return written;
    }

    return (*__orig_fwrite)(data, size, count, stream);
}

size_t fwrite_unlocked(const void *data, size_t size, size_t count, FILE *stream) {
    unsigned char *p = (unsigned char *) data;
    const int fd = fileno(stream);
    ssize_t skip = 0, written = 0;

    if ((size * count) == 0)
        return 0;

    if (should_colorize(fd)) {
        do {
            set_terminal_color(fd, 0.25, written);
            skip = skip_ansi_color_codes(p + written);
            written += (skip == 0) ? (*__orig_fwrite_unlocked)(p + written, 1, 1, stream) : skip;
            fflush(stream);
        } while(written < (size * count));

        reset_terminal_color(fd);

        return written;
    }

    return (*__orig_fwrite_unlocked)(data, size, count, stream);
}

int vfprintf(FILE *stream, const char *format, va_list ap) {
    char *buffer = NULL;
    size_t written = -1;

    if (vasprintf(&buffer, format, ap) > 0) {
        written = (*fwrite)(buffer, 1, strlen(buffer), stream);
        free(buffer);
    }

    return written;
}

int __fprintf_chk(FILE *stream, int flags, const char *format, ...) {
    va_list args;
    int result;

    va_start(args, format);
    result = (*vfprintf)(stream, format, args);
    va_end(args);

    return result;
}

int fprintf(FILE *stream, const char *format, ...) {
    va_list args;
    int result;

    va_start(args, format);
    result = (*vfprintf)(stream, format, args);
    va_end(args);

    return result;
}

int __printf_chk(int flags, const char *format, ...) {
    va_list args;
    int result;

    va_start(args, format);
    result = (*vfprintf)(stdout, format, args);
    va_end(args);

    return result;
}

int printf(const char *format, ...) {
    va_list args;
    int result;

    va_start(args, format);
    result = (*vfprintf)(stdout, format, args);
    va_end(args);

    return result;
}

int fputc(int chr, FILE *stream) {
    const unsigned char c = (unsigned char) chr;
    return (*fwrite)(&c, 1, 1, stream);
}

int fputc_unlocked(int chr, FILE *stream) {
    const unsigned char c = (unsigned char) chr;
    return (*fwrite_unlocked)(&c, 1, 1, stream);
}

int fputs(const char *str, FILE *stream) {
    return (*fwrite)(str, 1, strlen(str), stream);
}

int fputs_unlocked(const char *str, FILE *stream) {
    return (*fwrite_unlocked)(str, 1, strlen(str), stream);
}

int puts(const char *s) {
    int written = (*fputs)(s, stdout);
    (*fputc)('\n', stdout); /* WAT */
    return written;
}

int putchar(int c) {
    return (*fputc)(c, stdout);
}

__attribute__((constructor))
static void colorify(void) {
    /* All programs to blacklist from colorization at runtime. */
    regcomp(&blacklist_regex, "^(less|pager|vi)$", REG_NOSUB | REG_EXTENDED);

    /* Compile a regexp to match ANSI color escape codes. */
    regcomp(&ansi_escape_regex, "^(\x1b\\[[0-9;]+[mK])", REG_EXTENDED);

    INITIALIZE_HOOK(write);

    INITIALIZE_HOOK(__printf_chk);
    INITIALIZE_HOOK(printf);

    INITIALIZE_HOOK(fwrite);
    INITIALIZE_HOOK(fwrite_unlocked);

    INITIALIZE_HOOK(__fprintf_chk);
    INITIALIZE_HOOK(vfprintf);
    INITIALIZE_HOOK(fprintf);

    INITIALIZE_HOOK(fputc);
    INITIALIZE_HOOK(fputc_unlocked);

    INITIALIZE_HOOK(fputs);
    INITIALIZE_HOOK(fputs_unlocked);

    INITIALIZE_HOOK(putchar);
    INITIALIZE_HOOK(puts);
}

void _start(void) {
    INITIALIZE_HOOK(write);
    /* The magics. Let me show you them. */
    (*__orig_write)(STDOUT_FILENO, &__magic, __magic_len);
    _exit(9002);
}

