/*
 * aria_libc_posix — Low-level POSIX I/O and misc for Aria
 * Build: cc -O2 -shared -fPIC -o libaria_libc_posix.so aria_libc_posix.c
 */
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

/* ── File Descriptors ────────────────────────────────────────────── */

int64_t aria_libc_posix_open(const char *path, int32_t flags, int32_t mode) {
    if (!path) return -1;
    return (int64_t)open(path, flags, (mode_t)mode);
}

int32_t aria_libc_posix_close(int64_t fd) {
    if (fd < 0) return -1;
    return (int32_t)close((int)fd);
}

int64_t aria_libc_posix_read(int64_t fd, int64_t buf, int64_t count) {
    if (fd < 0 || !buf || count <= 0) return -1;
    return (int64_t)read((int)fd, (void *)(uintptr_t)buf, (size_t)count);
}

int64_t aria_libc_posix_write(int64_t fd, int64_t buf, int64_t count) {
    if (fd < 0 || !buf || count <= 0) return -1;
    return (int64_t)write((int)fd, (const void *)(uintptr_t)buf, (size_t)count);
}

int64_t aria_libc_posix_write_str(int64_t fd, const char *s) {
    if (fd < 0 || !s) return -1;
    return (int64_t)write((int)fd, s, strlen(s));
}

int64_t aria_libc_posix_lseek(int64_t fd, int64_t offset, int32_t whence) {
    if (fd < 0) return -1;
    return (int64_t)lseek((int)fd, (off_t)offset, whence);
}

/* ── Dup ─────────────────────────────────────────────────────────── */

int64_t aria_libc_posix_dup(int64_t fd) {
    return (int64_t)dup((int)fd);
}

int32_t aria_libc_posix_dup2(int64_t oldfd, int64_t newfd) {
    return (int32_t)dup2((int)oldfd, (int)newfd);
}

/* ── File Info ───────────────────────────────────────────────────── */

int32_t aria_libc_posix_access(const char *path, int32_t mode) {
    if (!path) return -1;
    return (int32_t)access(path, mode);
}

int32_t aria_libc_posix_file_exists(const char *path) {
    if (!path) return 0;
    return access(path, F_OK) == 0 ? 1 : 0;
}

int64_t aria_libc_posix_file_size(const char *path) {
    if (!path) return -1;
    struct stat st;
    if (stat(path, &st) != 0) return -1;
    return (int64_t)st.st_size;
}

int32_t aria_libc_posix_is_directory(const char *path) {
    if (!path) return 0;
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return S_ISDIR(st.st_mode) ? 1 : 0;
}

int32_t aria_libc_posix_is_file(const char *path) {
    if (!path) return 0;
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return S_ISREG(st.st_mode) ? 1 : 0;
}

int64_t aria_libc_posix_file_mtime(const char *path) {
    if (!path) return -1;
    struct stat st;
    if (stat(path, &st) != 0) return -1;
    return (int64_t)st.st_mtime;
}

int32_t aria_libc_posix_file_mode(const char *path) {
    if (!path) return -1;
    struct stat st;
    if (stat(path, &st) != 0) return -1;
    return (int32_t)(st.st_mode & 0777);
}

/* ── File Operations ─────────────────────────────────────────────── */

int32_t aria_libc_posix_unlink(const char *path) {
    if (!path) return -1;
    return (int32_t)unlink(path);
}

int32_t aria_libc_posix_rename(const char *old_path, const char *new_path) {
    if (!old_path || !new_path) return -1;
    return (int32_t)rename(old_path, new_path);
}

int32_t aria_libc_posix_mkdir(const char *path, int32_t mode) {
    if (!path) return -1;
    return (int32_t)mkdir(path, (mode_t)mode);
}

int32_t aria_libc_posix_rmdir(const char *path) {
    if (!path) return -1;
    return (int32_t)rmdir(path);
}

int32_t aria_libc_posix_chmod(const char *path, int32_t mode) {
    if (!path) return -1;
    return (int32_t)chmod(path, (mode_t)mode);
}

int32_t aria_libc_posix_link(const char *old_path, const char *new_path) {
    if (!old_path || !new_path) return -1;
    return (int32_t)link(old_path, new_path);
}

int32_t aria_libc_posix_symlink(const char *target, const char *linkpath) {
    if (!target || !linkpath) return -1;
    return (int32_t)symlink(target, linkpath);
}

static char g_link_buf[4096];

const char *aria_libc_posix_readlink(const char *path) {
    g_link_buf[0] = '\0';
    if (!path) return g_link_buf;
    ssize_t n = readlink(path, g_link_buf, sizeof(g_link_buf) - 1);
    if (n < 0) n = 0;
    g_link_buf[n] = '\0';
    return g_link_buf;
}

/* ── Open Flags ──────────────────────────────────────────────────── */

int32_t aria_libc_posix_O_RDONLY(void) { return O_RDONLY; }
int32_t aria_libc_posix_O_WRONLY(void) { return O_WRONLY; }
int32_t aria_libc_posix_O_RDWR(void)   { return O_RDWR; }
int32_t aria_libc_posix_O_CREAT(void)  { return O_CREAT; }
int32_t aria_libc_posix_O_TRUNC(void)  { return O_TRUNC; }
int32_t aria_libc_posix_O_APPEND(void) { return O_APPEND; }

/* ── Errno ───────────────────────────────────────────────────────── */

int32_t aria_libc_posix_errno(void) { return (int32_t)errno; }

const char *aria_libc_posix_strerror(int32_t err) {
    return strerror(err);
}
