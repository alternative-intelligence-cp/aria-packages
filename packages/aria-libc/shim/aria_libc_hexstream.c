/*
 * aria_libc_hexstream — AriaX Hexstream (fd 3-5) I/O for Aria
 * Build: cc -O2 -shared -fPIC -o libaria_libc_hexstream.so aria_libc_hexstream.c
 *
 * Six-stream topology:
 *   FD 0 = stdin (standard)
 *   FD 1 = stdout (standard)
 *   FD 2 = stderr (standard)
 *   FD 3 = stddbg  (debug output)
 *   FD 4 = stddati (data input)
 *   FD 5 = stddato (data output)
 */
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>

/* ── Init ────────────────────────────────────────────────────────── */

int32_t aria_libc_hexstream_init(void) {
    /* Ensure FDs 3-5 are open. If not, open /dev/null as placeholder. */
    for (int fd = 3; fd <= 5; fd++) {
        if (fcntl(fd, F_GETFD) == -1) {
            int nfd = open("/dev/null", (fd == 4) ? O_RDONLY : O_WRONLY);
            if (nfd >= 0 && nfd != fd) {
                dup2(nfd, fd);
                close(nfd);
            }
        }
    }
    return 0;
}

/* ── Redirect ────────────────────────────────────────────────────── */

int32_t aria_libc_hexstream_redirect_to_file(int32_t fd, const char *path) {
    if (!path) return -1;
    int nfd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (nfd < 0) return -1;
    if (dup2(nfd, fd) < 0) { close(nfd); return -1; }
    close(nfd);
    return 0;
}

int32_t aria_libc_hexstream_redirect_from_file(int32_t fd, const char *path) {
    if (!path) return -1;
    int nfd = open(path, O_RDONLY);
    if (nfd < 0) return -1;
    if (dup2(nfd, fd) < 0) { close(nfd); return -1; }
    close(nfd);
    return 0;
}

/* ── Write ───────────────────────────────────────────────────────── */

int32_t aria_libc_hexstream_write(int32_t fd, const char *data) {
    if (!data) return -1;
    size_t len = strlen(data);
    ssize_t n = write(fd, data, len);
    return (n >= 0) ? (int32_t)n : -1;
}

int32_t aria_libc_hexstream_debug(const char *msg) {
    return aria_libc_hexstream_write(3, msg);
}

int32_t aria_libc_hexstream_write_int64(int32_t fd, int64_t value) {
    char buf[32];
    int len = snprintf(buf, sizeof(buf), "%ld\n", (long)value);
    ssize_t n = write(fd, buf, (size_t)len);
    return (n >= 0) ? 0 : -1;
}

/* ── Read ────────────────────────────────────────────────────────── */

static char g_line_buf[4096];

const char *aria_libc_hexstream_read_line(int32_t fd) {
    int pos = 0;
    char ch;
    while (pos < (int)(sizeof(g_line_buf) - 1)) {
        ssize_t n = read(fd, &ch, 1);
        if (n <= 0) break;
        g_line_buf[pos++] = ch;
        if (ch == '\n') break;
    }
    g_line_buf[pos] = '\0';
    return g_line_buf;
}

int64_t aria_libc_hexstream_read_int64(int32_t fd) {
    const char *line = aria_libc_hexstream_read_line(fd);
    return (int64_t)atol(line);
}

/* ── Query ───────────────────────────────────────────────────────── */

int32_t aria_libc_hexstream_is_open(int32_t fd) {
    return (fcntl(fd, F_GETFD) != -1) ? 1 : 0;
}

int32_t aria_libc_hexstream_fd(const char *name) {
    if (!name) return -1;
    if (strcmp(name, "stdin")   == 0) return 0;
    if (strcmp(name, "stdout")  == 0) return 1;
    if (strcmp(name, "stderr")  == 0) return 2;
    if (strcmp(name, "stddbg")  == 0) return 3;
    if (strcmp(name, "stddati") == 0) return 4;
    if (strcmp(name, "stddato") == 0) return 5;
    return -1;
}
