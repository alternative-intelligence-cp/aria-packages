/*
 * aria_static_shim.c — Static file serving for Aria HTTP servers
 *
 * Supports:
 *   - Serve files from a configured root directory
 *   - MIME type detection by extension
 *   - Path traversal protection (no ../ escapes)
 *   - Read file contents into buffer
 *   - Directory index (index.html)
 *
 * Thread-safety: single-threaded (global state).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>

/* ── limits ──────────────────────────────────────────────────────── */
#define MAX_PATH_LEN       4096
#define MAX_FILE_SIZE      (1024 * 1024)  /* 1 MB max file */
#define MAX_MIME_LEN       128

/* ── state ───────────────────────────────────────────────────────── */
static char   g_root[MAX_PATH_LEN]     = "./public";
static char   g_index_file[256]        = "index.html";
static char   g_resolved[MAX_PATH_LEN] = "";
static char   g_mime[MAX_MIME_LEN]     = "";
static char  *g_file_buf               = NULL;
static int    g_file_len               = 0;
static char   g_error[512]            = "";

/* test tracking */
static int g_tests_passed = 0;
static int g_tests_failed = 0;

/* ── MIME type lookup ────────────────────────────────────────────── */

typedef struct {
    const char *ext;
    const char *mime;
} MimeEntry;

static const MimeEntry g_mime_table[] = {
    { ".html",  "text/html" },
    { ".htm",   "text/html" },
    { ".css",   "text/css" },
    { ".js",    "application/javascript" },
    { ".json",  "application/json" },
    { ".xml",   "application/xml" },
    { ".txt",   "text/plain" },
    { ".md",    "text/markdown" },
    { ".csv",   "text/csv" },
    { ".png",   "image/png" },
    { ".jpg",   "image/jpeg" },
    { ".jpeg",  "image/jpeg" },
    { ".gif",   "image/gif" },
    { ".svg",   "image/svg+xml" },
    { ".ico",   "image/x-icon" },
    { ".webp",  "image/webp" },
    { ".woff",  "font/woff" },
    { ".woff2", "font/woff2" },
    { ".ttf",   "font/ttf" },
    { ".otf",   "font/otf" },
    { ".pdf",   "application/pdf" },
    { ".zip",   "application/zip" },
    { ".gz",    "application/gzip" },
    { ".wasm",  "application/wasm" },
    { ".mp3",   "audio/mpeg" },
    { ".mp4",   "video/mp4" },
    { ".webm",  "video/webm" },
    { NULL,     NULL }
};

/* ── helpers ─────────────────────────────────────────────────────── */

static void set_error(const char *msg) {
    snprintf(g_error, sizeof(g_error), "%s", msg);
}

/* Get file extension (including dot). Returns "" if none. */
static const char *get_ext(const char *path) {
    const char *dot = strrchr(path, '.');
    const char *slash = strrchr(path, '/');
    if (!dot) return "";
    if (slash && dot < slash) return ""; /* dot in directory name, not file */
    return dot;
}

/* Detect MIME type from file extension. */
static const char *detect_mime(const char *path) {
    const char *ext = get_ext(path);
    if (!ext || !*ext) return "application/octet-stream";

    for (int i = 0; g_mime_table[i].ext; i++) {
        if (strcasecmp(ext, g_mime_table[i].ext) == 0) {
            return g_mime_table[i].mime;
        }
    }
    return "application/octet-stream";
}

/*
 * Validate and resolve a URL path against the root directory.
 * Returns 1 if safe and file exists, 0 otherwise.
 * Prevents path traversal attacks.
 */
static int safe_resolve(const char *url_path) {
    g_resolved[0] = '\0';
    g_error[0] = '\0';

    if (!url_path || !*url_path) {
        set_error("Empty path");
        return 0;
    }

    /* reject paths with .. sequences */
    if (strstr(url_path, "..") != NULL) {
        set_error("Path traversal detected");
        return 0;
    }

    /* build full path */
    char candidate[MAX_PATH_LEN];
    if (url_path[0] == '/') {
        snprintf(candidate, sizeof(candidate), "%s%s", g_root, url_path);
    } else {
        snprintf(candidate, sizeof(candidate), "%s/%s", g_root, url_path);
    }

    /* resolve to absolute path */
    char real[PATH_MAX];
    if (!realpath(candidate, real)) {
        /* file doesn't exist — try with index file if path ends with / */
        size_t len = strlen(candidate);
        if (len > 0 && candidate[len-1] == '/') {
            snprintf(candidate + len, sizeof(candidate) - len, "%s", g_index_file);
            if (!realpath(candidate, real)) {
                set_error("File not found");
                return 0;
            }
        } else {
            set_error("File not found");
            return 0;
        }
    }

    /* resolve root to absolute */
    char real_root[PATH_MAX];
    if (!realpath(g_root, real_root)) {
        set_error("Root directory not found");
        return 0;
    }

    /* verify resolved path is under root */
    size_t root_len = strlen(real_root);
    if (strncmp(real, real_root, root_len) != 0) {
        set_error("Path traversal detected");
        return 0;
    }

    /* check it's a regular file */
    struct stat st;
    if (stat(real, &st) != 0) {
        set_error("File not found");
        return 0;
    }

    if (S_ISDIR(st.st_mode)) {
        /* try index file */
        snprintf(candidate, sizeof(candidate), "%s/%s", real, g_index_file);
        if (!realpath(candidate, real)) {
            set_error("Directory without index");
            return 0;
        }
        if (stat(real, &st) != 0 || !S_ISREG(st.st_mode)) {
            set_error("Index file not found");
            return 0;
        }
    } else if (!S_ISREG(st.st_mode)) {
        set_error("Not a regular file");
        return 0;
    }

    if (st.st_size > MAX_FILE_SIZE) {
        set_error("File too large");
        return 0;
    }

    snprintf(g_resolved, sizeof(g_resolved), "%s", real);
    return 1;
}

/* ── configuration ───────────────────────────────────────────────── */

void aria_static_set_root(const char *root) {
    if (root && *root) {
        snprintf(g_root, sizeof(g_root), "%s", root);
    }
}

const char *aria_static_get_root(void) {
    return g_root;
}

void aria_static_set_index(const char *filename) {
    if (filename && *filename) {
        snprintf(g_index_file, sizeof(g_index_file), "%s", filename);
    }
}

/* ── file serving API ────────────────────────────────────────────── */

/*
 * Resolve a URL path to a file. Returns 1 if found, 0 if not.
 * On success, sets internal state for mime_type(), file_read(), etc.
 */
int32_t aria_static_resolve(const char *url_path) {
    if (!safe_resolve(url_path)) return 0;
    snprintf(g_mime, sizeof(g_mime), "%s", detect_mime(g_resolved));
    return 1;
}

/* Get MIME type of resolved file. */
const char *aria_static_mime_type(void) {
    return g_mime;
}

/*
 * Read the resolved file into an internal buffer.
 * Returns file size in bytes, or -1 on error.
 */
int32_t aria_static_read(void) {
    if (!g_resolved[0]) {
        set_error("No file resolved");
        return -1;
    }

    FILE *f = fopen(g_resolved, "rb");
    if (!f) {
        set_error("Cannot open file");
        return -1;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size < 0 || size > MAX_FILE_SIZE) {
        fclose(f);
        set_error("File too large");
        return -1;
    }

    /* (re)allocate buffer */
    if (g_file_buf) { free(g_file_buf); g_file_buf = NULL; }
    g_file_buf = malloc((size_t)size + 1);
    if (!g_file_buf) {
        fclose(f);
        set_error("Out of memory");
        return -1;
    }

    size_t nread = fread(g_file_buf, 1, (size_t)size, f);
    fclose(f);

    g_file_buf[nread] = '\0';
    g_file_len = (int)nread;
    return (int32_t)nread;
}

/* Get the file contents as a string (after aria_static_read). */
const char *aria_static_content(void) {
    if (!g_file_buf) return "";
    return g_file_buf;
}

/* Get file length (after aria_static_read). */
int32_t aria_static_length(void) {
    return (int32_t)g_file_len;
}

/* Get resolved absolute path. */
const char *aria_static_resolved_path(void) {
    return g_resolved;
}

/* Get MIME type for any path (without resolving). */
const char *aria_static_detect_mime(const char *path) {
    snprintf(g_mime, sizeof(g_mime), "%s", detect_mime(path));
    return g_mime;
}

/* Get error message. */
const char *aria_static_error(void) {
    return g_error;
}

/* Reset state. */
void aria_static_reset(void) {
    g_resolved[0] = '\0';
    g_mime[0]     = '\0';
    g_error[0]    = '\0';
    g_file_len    = 0;
    if (g_file_buf) { free(g_file_buf); g_file_buf = NULL; }
    snprintf(g_root, sizeof(g_root), "./public");
    snprintf(g_index_file, sizeof(g_index_file), "index.html");
}

/* ── test helpers ────────────────────────────────────────────────── */

void aria_static_assert_str_eq(const char *label, const char *expected, const char *actual) {
    if (strcmp(expected, actual) == 0) {
        printf("  PASS: %s\n", label);
        g_tests_passed++;
    } else {
        printf("  FAIL: %s\n    expected: \"%s\"\n    got:      \"%s\"\n", label, expected, actual);
        g_tests_failed++;
    }
}

void aria_static_assert_int_eq(const char *label, int32_t expected, int32_t actual) {
    if (expected == actual) {
        printf("  PASS: %s\n", label);
        g_tests_passed++;
    } else {
        printf("  FAIL: %s — expected %d, got %d\n", label, expected, actual);
        g_tests_failed++;
    }
}

void aria_static_assert_contains(const char *label, const char *expected, const char *actual) {
    if (strstr(actual, expected) != NULL) {
        printf("  PASS: %s\n", label);
        g_tests_passed++;
    } else {
        printf("  FAIL: %s\n    expected to contain: \"%s\"\n    in: \"%s\"\n", label, expected, actual);
        g_tests_failed++;
    }
}

void aria_static_test_summary(void) {
    printf("\n--- aria-static: %d passed, %d failed ---\n",
           g_tests_passed, g_tests_failed);
    g_tests_passed = 0;
    g_tests_failed = 0;
}
