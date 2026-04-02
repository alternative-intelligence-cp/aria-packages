/*
 * aria_libc_fs — Directory traversal and filesystem ops for Aria
 * Build: cc -O2 -shared -fPIC -o libaria_libc_fs.so aria_libc_fs.c
 */
#include <dirent.h>
#include <sys/stat.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

/* ── Directory Listing ───────────────────────────────────────────── */

static DIR *g_dir = NULL;

int32_t aria_libc_fs_opendir(const char *path) {
    if (g_dir) { closedir(g_dir); g_dir = NULL; }
    if (!path) return -1;
    g_dir = opendir(path);
    return g_dir ? 0 : -1;
}

static char g_name_buf[1024];

const char *aria_libc_fs_readdir(void) {
    g_name_buf[0] = '\0';
    if (!g_dir) return g_name_buf;
    struct dirent *ent = readdir(g_dir);
    if (!ent) return g_name_buf;
    size_t len = strlen(ent->d_name);
    if (len >= sizeof(g_name_buf)) len = sizeof(g_name_buf) - 1;
    memcpy(g_name_buf, ent->d_name, len);
    g_name_buf[len] = '\0';
    return g_name_buf;
}

int32_t aria_libc_fs_closedir(void) {
    if (!g_dir) return -1;
    int r = closedir(g_dir);
    g_dir = NULL;
    return (int32_t)r;
}

/* ── Multi-handle directory (up to 16 concurrent) ────────────────── */

static DIR *g_dirs[16] = {0};

int32_t aria_libc_fs_dir_open(const char *path) {
    if (!path) return -1;
    for (int i = 0; i < 16; i++) {
        if (!g_dirs[i]) {
            g_dirs[i] = opendir(path);
            return g_dirs[i] ? i : -1;
        }
    }
    return -1;
}

const char *aria_libc_fs_dir_read(int32_t handle) {
    g_name_buf[0] = '\0';
    if (handle < 0 || handle >= 16 || !g_dirs[handle]) return g_name_buf;
    struct dirent *ent = readdir(g_dirs[handle]);
    if (!ent) return g_name_buf;
    size_t len = strlen(ent->d_name);
    if (len >= sizeof(g_name_buf)) len = sizeof(g_name_buf) - 1;
    memcpy(g_name_buf, ent->d_name, len);
    g_name_buf[len] = '\0';
    return g_name_buf;
}

int32_t aria_libc_fs_dir_close(int32_t handle) {
    if (handle < 0 || handle >= 16 || !g_dirs[handle]) return -1;
    int r = closedir(g_dirs[handle]);
    g_dirs[handle] = NULL;
    return (int32_t)r;
}

int32_t aria_libc_fs_dir_entry_type(int32_t handle) {
    /* Returns: 0=unknown, 1=file, 2=dir, 3=symlink */
    /* Must be called RIGHT AFTER readdir — uses last entry */
    (void)handle;
    /* Can't reliably get this from readdir on all platforms,
       use stat-based approach instead */
    return 0;
}

/* ── Stat-based type check ───────────────────────────────────────── */

int32_t aria_libc_fs_entry_type(const char *path) {
    if (!path) return 0;
    struct stat st;
    if (lstat(path, &st) != 0) return 0;
    if (S_ISREG(st.st_mode)) return 1;
    if (S_ISDIR(st.st_mode)) return 2;
    if (S_ISLNK(st.st_mode)) return 3;
    return 0;
}

/* ── Path Operations ─────────────────────────────────────────────── */

static char g_path_buf[4096];

const char *aria_libc_fs_join_path(const char *a, const char *b) {
    g_path_buf[0] = '\0';
    if (!a || !b) return g_path_buf;
    size_t la = strlen(a);
    size_t lb = strlen(b);
    if (la + lb + 2 >= sizeof(g_path_buf)) return g_path_buf;
    memcpy(g_path_buf, a, la);
    if (la > 0 && a[la - 1] != '/') {
        g_path_buf[la] = '/';
        la++;
    }
    memcpy(g_path_buf + la, b, lb);
    g_path_buf[la + lb] = '\0';
    return g_path_buf;
}

const char *aria_libc_fs_basename(const char *path) {
    g_path_buf[0] = '\0';
    if (!path) return g_path_buf;
    const char *last = strrchr(path, '/');
    const char *name = last ? last + 1 : path;
    size_t len = strlen(name);
    if (len >= sizeof(g_path_buf)) len = sizeof(g_path_buf) - 1;
    memcpy(g_path_buf, name, len);
    g_path_buf[len] = '\0';
    return g_path_buf;
}

const char *aria_libc_fs_dirname(const char *path) {
    g_path_buf[0] = '\0';
    if (!path) return g_path_buf;
    size_t len = strlen(path);
    if (len >= sizeof(g_path_buf)) len = sizeof(g_path_buf) - 1;
    memcpy(g_path_buf, path, len);
    g_path_buf[len] = '\0';
    /* Find last slash */
    char *last = strrchr(g_path_buf, '/');
    if (last) *last = '\0';
    else { g_path_buf[0] = '.'; g_path_buf[1] = '\0'; }
    return g_path_buf;
}

const char *aria_libc_fs_extension(const char *path) {
    g_path_buf[0] = '\0';
    if (!path) return g_path_buf;
    const char *dot = strrchr(path, '.');
    if (!dot || dot == path) return g_path_buf;
    size_t len = strlen(dot);
    if (len >= sizeof(g_path_buf)) len = sizeof(g_path_buf) - 1;
    memcpy(g_path_buf, dot, len);
    g_path_buf[len] = '\0';
    return g_path_buf;
}

const char *aria_libc_fs_realpath(const char *path) {
    g_path_buf[0] = '\0';
    if (!path) return g_path_buf;
    char *r = realpath(path, g_path_buf);
    if (!r) g_path_buf[0] = '\0';
    return g_path_buf;
}

/* ── Tree Walk (flat, one level) ─────────────────────────────────── */

static char g_tree_buf[65536];

const char *aria_libc_fs_list_dir(const char *path) {
    g_tree_buf[0] = '\0';
    if (!path) return g_tree_buf;
    DIR *d = opendir(path);
    if (!d) return g_tree_buf;
    size_t pos = 0;
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;
        size_t nlen = strlen(ent->d_name);
        if (pos + nlen + 2 >= sizeof(g_tree_buf)) break;
        memcpy(g_tree_buf + pos, ent->d_name, nlen);
        pos += nlen;
        g_tree_buf[pos++] = '\n';
    }
    if (pos > 0) pos--;  /* remove trailing newline */
    g_tree_buf[pos] = '\0';
    closedir(d);
    return g_tree_buf;
}
