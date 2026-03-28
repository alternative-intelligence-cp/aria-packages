/*
 * aria_aifs_shim.c — High-level AI filesystem utilities
 *
 * Provides:
 * - Batch annotation: annotate all model files in a directory at once
 * - Model catalog: build and query an in-memory index of annotated files
 * - Integrity suite: compute + verify checksums for all files in a directory
 * - Streaming reader: chunked sequential reader with prefetch
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/xattr.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

typedef struct { char* data; int64_t length; } AriaString;

static AriaString make_str(const char* s) {
    AriaString r;
    r.length = (int64_t)strlen(s);
    r.data = (char*)malloc((size_t)(r.length + 1));
    memcpy(r.data, s, (size_t)(r.length + 1));
    return r;
}

#define MAX_PATH 1024

/* ═══════════════════ format detection (local copy) ═══════════════════ */

static const char* detect_format_local(const char* path) {
    unsigned char magic[16] = {0};
    int fd = open(path, O_RDONLY);
    if (fd < 0) return "unknown";
    ssize_t n = read(fd, magic, 16);
    close(fd);
    if (n < 4) return "unknown";

    if (magic[0]=='G' && magic[1]=='G' && magic[2]=='U' && magic[3]=='F') return "gguf";
    if (n >= 8 && magic[0]==0x89 && magic[1]=='H' && magic[2]=='D' && magic[3]=='F') return "hdf5";
    if (n >= 6 && magic[0]==0x93 && magic[1]=='N' && magic[2]=='U' && magic[3]=='M') return "numpy";
    if (magic[0]=='P' && magic[1]=='K' && magic[2]==0x03 && magic[3]==0x04) return "pytorch";
    if (n >= 9 && magic[8] == '{') return "safetensors";
    if (n >= 8 && magic[4]=='T' && magic[5]=='F' && magic[6]=='L' && magic[7]=='3') return "tflite";

    const char* dot = strrchr(path, '.');
    if (dot) {
        if (strcmp(dot, ".onnx") == 0) return "onnx";
        if (strcmp(dot, ".safetensors") == 0) return "safetensors";
        if (strcmp(dot, ".gguf") == 0) return "gguf";
        if (strcmp(dot, ".pt") == 0 || strcmp(dot, ".pth") == 0) return "pytorch";
        if (strcmp(dot, ".h5") == 0 || strcmp(dot, ".hdf5") == 0) return "hdf5";
        if (strcmp(dot, ".npy") == 0 || strcmp(dot, ".npz") == 0) return "numpy";
        if (strcmp(dot, ".tflite") == 0) return "tflite";
        if (strcmp(dot, ".engine") == 0 || strcmp(dot, ".trt") == 0) return "tensorrt";
    }
    return "unknown";
}

/* ═══════════════════ batch annotation ═══════════════════ */

/*
 * Scan a directory, auto-detect format for each file, and store as xattr.
 * Returns the number of files annotated, or -1 on error.
 */
int32_t aria_aifs_batch_annotate(const char* dir_path) {
    DIR* d = opendir(dir_path);
    if (!d) return -1;

    int32_t count = 0;
    struct dirent* ent;
    while ((ent = readdir(d)) != NULL) {
        if (ent->d_name[0] == '.') continue;

        char fullpath[MAX_PATH];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", dir_path, ent->d_name);

        struct stat st;
        if (stat(fullpath, &st) != 0 || !S_ISREG(st.st_mode)) continue;

        const char* fmt = detect_format_local(fullpath);
        if (strcmp(fmt, "unknown") != 0) {
            setxattr(fullpath, "user.model.format", fmt, strlen(fmt), 0);
            count++;
        }
    }
    closedir(d);
    return count;
}

/* ═══════════════════ batch checksum ═══════════════════ */

/*
 * Compute SHA-256 checksums for all regular files in a directory and store as xattrs.
 * Returns number of files checksummed, or -1 on error.
 */
int32_t aria_aifs_batch_checksum(const char* dir_path) {
    DIR* d = opendir(dir_path);
    if (!d) return -1;

    int32_t count = 0;
    struct dirent* ent;
    while ((ent = readdir(d)) != NULL) {
        if (ent->d_name[0] == '.') continue;

        char fullpath[MAX_PATH];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", dir_path, ent->d_name);

        struct stat st;
        if (stat(fullpath, &st) != 0 || !S_ISREG(st.st_mode)) continue;

        char cmd[MAX_PATH + 32];
        snprintf(cmd, sizeof(cmd), "sha256sum '%s' 2>/dev/null", fullpath);
        FILE* fp = popen(cmd, "r");
        if (!fp) continue;

        char hash[128] = {0};
        if (fgets(hash, sizeof(hash), fp) != NULL && strlen(hash) >= 64) {
            hash[64] = '\0';
            setxattr(fullpath, "user.checksum.sha256", hash, 64, 0);
            count++;
        }
        pclose(fp);
    }
    closedir(d);
    return count;
}

/* ═══════════════════ batch verify ═══════════════════ */

/*
 * Verify checksums for all files in a directory that have stored checksums.
 * Returns number of files that PASSED verification.
 * Files without stored checksums are skipped.
 * Sets g_verify_failed to count of mismatches.
 */
static int32_t g_verify_failed = 0;

int32_t aria_aifs_batch_verify(const char* dir_path) {
    DIR* d = opendir(dir_path);
    if (!d) return -1;

    int32_t passed = 0;
    g_verify_failed = 0;

    struct dirent* ent;
    while ((ent = readdir(d)) != NULL) {
        if (ent->d_name[0] == '.') continue;

        char fullpath[MAX_PATH];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", dir_path, ent->d_name);

        struct stat st;
        if (stat(fullpath, &st) != 0 || !S_ISREG(st.st_mode)) continue;

        // Get stored checksum
        char stored[128] = {0};
        ssize_t slen = getxattr(fullpath, "user.checksum.sha256", stored, sizeof(stored) - 1);
        if (slen < 64) continue; // no checksum stored
        stored[64] = '\0';

        // Compute fresh
        char cmd[MAX_PATH + 32];
        snprintf(cmd, sizeof(cmd), "sha256sum '%s' 2>/dev/null", fullpath);
        FILE* fp = popen(cmd, "r");
        if (!fp) { g_verify_failed++; continue; }

        char fresh[128] = {0};
        if (fgets(fresh, sizeof(fresh), fp) != NULL && strlen(fresh) >= 64) {
            fresh[64] = '\0';
            if (memcmp(stored, fresh, 64) == 0) passed++;
            else g_verify_failed++;
        } else {
            g_verify_failed++;
        }
        pclose(fp);
    }
    closedir(d);
    return passed;
}

int32_t aria_aifs_batch_verify_failed(void) {
    return g_verify_failed;
}

/* ═══════════════════ streaming reader ═══════════════════ */

/*
 * A simple streaming reader that opens a file in sequential mode,
 * reads it in fixed-size chunks, and returns total bytes read.
 * Useful for benchmarking sequential throughput.
 */
int64_t aria_aifs_stream_read_all(const char* path, int64_t chunk_size) {
    if (chunk_size <= 0) chunk_size = 65536; // default 64KB

    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;

    struct stat st;
    if (fstat(fd, &st) == 0) {
        posix_fadvise(fd, 0, st.st_size, POSIX_FADV_SEQUENTIAL);
    }

    char* buf = (char*)malloc((size_t)chunk_size);
    if (!buf) { close(fd); return -1; }

    int64_t total = 0;
    ssize_t n;
    while ((n = read(fd, buf, (size_t)chunk_size)) > 0) {
        total += (int64_t)n;
    }

    free(buf);
    close(fd);
    return total;
}

/* ═══════════════════ annotated file count ═══════════════════ */

/*
 * Count how many files in a directory have AI metadata (any xattr set).
 */
int32_t aria_aifs_count_annotated(const char* dir_path) {
    DIR* d = opendir(dir_path);
    if (!d) return -1;

    int32_t count = 0;
    struct dirent* ent;
    while ((ent = readdir(d)) != NULL) {
        if (ent->d_name[0] == '.') continue;

        char fullpath[MAX_PATH];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", dir_path, ent->d_name);

        struct stat st;
        if (stat(fullpath, &st) != 0 || !S_ISREG(st.st_mode)) continue;

        char buf[4] = {0};
        ssize_t fl = getxattr(fullpath, "user.model.format", buf, 1);
        ssize_t dl = getxattr(fullpath, "user.tensor.dtype", buf, 1);
        ssize_t sl = getxattr(fullpath, "user.tensor.shape", buf, 1);
        ssize_t cl = getxattr(fullpath, "user.checksum.sha256", buf, 1);

        if (fl > 0 || dl > 0 || sl > 0 || cl > 0) count++;
    }
    closedir(d);
    return count;
}
