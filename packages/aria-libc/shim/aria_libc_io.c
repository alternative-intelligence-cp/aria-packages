/*
 * aria_libc_io — File I/O operations for Aria
 * Build: cc -O2 -shared -fPIC -o libaria_libc_io.so aria_libc_io.c
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

/* Handles are int64 to dodge Aria's {i1, ptr} optional-wrapper bug */

int64_t aria_libc_io_fopen(const char *path, const char *mode) {
    if (!path || !mode) return 0;
    FILE *f = fopen(path, mode);
    return (int64_t)(uintptr_t)f;
}

int32_t aria_libc_io_fclose(int64_t handle) {
    if (!handle) return -1;
    return (int32_t)fclose((FILE *)(uintptr_t)handle);
}

int64_t aria_libc_io_fread(int64_t buf, int64_t size, int64_t count, int64_t handle) {
    if (!handle || !buf) return 0;
    return (int64_t)fread((void *)(uintptr_t)buf, (size_t)size, (size_t)count,
                          (FILE *)(uintptr_t)handle);
}

int64_t aria_libc_io_fwrite(int64_t buf, int64_t size, int64_t count, int64_t handle) {
    if (!handle || !buf) return 0;
    return (int64_t)fwrite((const void *)(uintptr_t)buf, (size_t)size, (size_t)count,
                           (FILE *)(uintptr_t)handle);
}

/* Read a line into static buffer — returns empty string on EOF */
static char g_line_buf[65536];

const char *aria_libc_io_fgets(int64_t handle) {
    g_line_buf[0] = '\0';
    if (!handle) return g_line_buf;
    char *r = fgets(g_line_buf, sizeof(g_line_buf), (FILE *)(uintptr_t)handle);
    if (!r) g_line_buf[0] = '\0';
    return g_line_buf;
}

int32_t aria_libc_io_feof(int64_t handle) {
    if (!handle) return 1;
    return feof((FILE *)(uintptr_t)handle) ? 1 : 0;
}

int32_t aria_libc_io_fflush(int64_t handle) {
    if (!handle) return fflush(stdout);
    return (int32_t)fflush((FILE *)(uintptr_t)handle);
}

int32_t aria_libc_io_fseek(int64_t handle, int64_t offset, int32_t whence) {
    if (!handle) return -1;
    return (int32_t)fseek((FILE *)(uintptr_t)handle, (long)offset, whence);
}

int64_t aria_libc_io_ftell(int64_t handle) {
    if (!handle) return -1;
    return (int64_t)ftell((FILE *)(uintptr_t)handle);
}

/* Write a string directly to a file handle */
int32_t aria_libc_io_fputs(const char *s, int64_t handle) {
    if (!handle || !s) return -1;
    return (int32_t)fputs(s, (FILE *)(uintptr_t)handle);
}

/* Write string to stdout */
int32_t aria_libc_io_puts(const char *s) {
    if (!s) return -1;
    return (int32_t)puts(s);
}

/* fprintf with a single string arg */
int32_t aria_libc_io_fprintf_s(int64_t handle, const char *fmt, const char *arg) {
    if (!handle || !fmt) return -1;
    return (int32_t)fprintf((FILE *)(uintptr_t)handle, fmt, arg ? arg : "");
}

/* fprintf with a single int64 arg */
int32_t aria_libc_io_fprintf_d(int64_t handle, const char *fmt, int64_t arg) {
    if (!handle || !fmt) return -1;
    return (int32_t)fprintf((FILE *)(uintptr_t)handle, fmt, arg);
}

/* Write string to file by path — convenience */
int32_t aria_libc_io_write_file(const char *path, const char *content) {
    if (!path || !content) return -1;
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    size_t len = strlen(content);
    size_t written = fwrite(content, 1, len, f);
    fclose(f);
    return (written == len) ? 0 : -1;
}

/* Read entire file into static buffer */
static char g_read_buf[262144];  /* 256 KB */

const char *aria_libc_io_read_file(const char *path) {
    g_read_buf[0] = '\0';
    if (!path) return g_read_buf;
    FILE *f = fopen(path, "r");
    if (!f) return g_read_buf;
    size_t n = fread(g_read_buf, 1, sizeof(g_read_buf) - 1, f);
    g_read_buf[n] = '\0';
    fclose(f);
    return g_read_buf;
}

/* Get stdin/stdout/stderr handles */
int64_t aria_libc_io_stdin(void)  { return (int64_t)(uintptr_t)stdin; }
int64_t aria_libc_io_stdout(void) { return (int64_t)(uintptr_t)stdout; }
int64_t aria_libc_io_stderr(void) { return (int64_t)(uintptr_t)stderr; }

/* SEEK constants */
int32_t aria_libc_io_seek_set(void) { return SEEK_SET; }
int32_t aria_libc_io_seek_cur(void) { return SEEK_CUR; }
int32_t aria_libc_io_seek_end(void) { return SEEK_END; }
