/*
 * aria_editor_shim.c — Line buffer management for Aria text editor
 *
 * Provides efficient line-based text storage backed by a dynamic
 * char** array. Each line is a heap-allocated C string (no trailing \n).
 *
 * Build:
 *   cc -O2 -shared -fPIC -Wall -o libaria_editor_shim.so aria_editor_shim.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- internal state ---- */
static char **lines     = NULL;
static int    line_count = 0;
static int    capacity   = 0;
static int    dirty      = 0;   /* 1 if buffer modified since last load/save */

/* ---- helpers ---- */
static void ensure_capacity(int need) {
    if (need <= capacity) return;
    int new_cap = capacity ? capacity * 2 : 64;
    while (new_cap < need) new_cap *= 2;
    char **tmp = realloc(lines, sizeof(char*) * (size_t)new_cap);
    if (!tmp) return;
    lines    = tmp;
    capacity = new_cap;
}

static char *dup_str(const char *s) {
    if (!s) s = "";
    size_t len = strlen(s);
    char *d = malloc(len + 1);
    if (d) memcpy(d, s, len + 1);
    return d;
}

/* ---- public API ---- */

/* Initialise (or reset) to an empty buffer with one blank line */
void aria_editor_init(void) {
    for (int i = 0; i < line_count; i++) free(lines[i]);
    free(lines);
    lines      = NULL;
    line_count = 0;
    capacity   = 0;
    dirty      = 0;
    ensure_capacity(1);
    lines[0]   = dup_str("");
    line_count = 1;
}

/* Load content string into buffer, splitting on '\n'.
 * Replaces any existing content. */
void aria_editor_load(const char *content) {
    /* free old */
    for (int i = 0; i < line_count; i++) free(lines[i]);
    line_count = 0;

    if (!content || *content == '\0') {
        ensure_capacity(1);
        lines[0]   = dup_str("");
        line_count = 1;
        dirty      = 0;
        return;
    }

    const char *p = content;
    while (*p) {
        const char *nl = strchr(p, '\n');
        size_t len = nl ? (size_t)(nl - p) : strlen(p);
        ensure_capacity(line_count + 1);
        lines[line_count] = malloc(len + 1);
        if (lines[line_count]) {
            memcpy(lines[line_count], p, len);
            lines[line_count][len] = '\0';
        }
        line_count++;
        if (nl) p = nl + 1; else break;
    }
    /* If content ends with '\n', add trailing empty line */
    if (line_count > 0) {
        size_t clen = strlen(content);
        if (clen > 0 && content[clen - 1] == '\n') {
            ensure_capacity(line_count + 1);
            lines[line_count] = dup_str("");
            line_count++;
        }
    }
    if (line_count == 0) {
        ensure_capacity(1);
        lines[0]   = dup_str("");
        line_count = 1;
    }
    dirty = 0;
}

/* Number of lines in buffer */
int aria_editor_line_count(void) {
    return line_count;
}

/* Get line at index (0-based).  Returns "" for out-of-range. */
const char *aria_editor_get_line(int idx) {
    if (idx < 0 || idx >= line_count) return "";
    return lines[idx];
}

/* Get length of line at index */
int aria_editor_line_len(int idx) {
    if (idx < 0 || idx >= line_count) return 0;
    return (int)strlen(lines[idx]);
}

/* Replace entire line at index */
void aria_editor_set_line(int idx, const char *text) {
    if (idx < 0 || idx >= line_count) return;
    free(lines[idx]);
    lines[idx] = dup_str(text);
    dirty = 1;
}

/* Insert a new line at index, shifting lines down */
void aria_editor_insert_line(int idx, const char *text) {
    if (idx < 0) idx = 0;
    if (idx > line_count) idx = line_count;
    ensure_capacity(line_count + 1);
    memmove(&lines[idx + 1], &lines[idx],
            sizeof(char*) * (size_t)(line_count - idx));
    lines[idx] = dup_str(text);
    line_count++;
    dirty = 1;
}

/* Delete line at index */
void aria_editor_delete_line(int idx) {
    if (idx < 0 || idx >= line_count) return;
    free(lines[idx]);
    memmove(&lines[idx], &lines[idx + 1],
            sizeof(char*) * (size_t)(line_count - idx - 1));
    line_count--;
    /* Always keep at least one line */
    if (line_count == 0) {
        ensure_capacity(1);
        lines[0]   = dup_str("");
        line_count = 1;
    }
    dirty = 1;
}

/* Insert a character into a line at column position */
void aria_editor_insert_char(int row, int col, int ch) {
    if (row < 0 || row >= line_count) return;
    int len = (int)strlen(lines[row]);
    if (col < 0) col = 0;
    if (col > len) col = len;
    char *old = lines[row];
    char *nw  = malloc((size_t)(len + 2));
    if (!nw) return;
    memcpy(nw, old, (size_t)col);
    nw[col] = (char)ch;
    memcpy(nw + col + 1, old + col, (size_t)(len - col + 1));
    lines[row] = nw;
    free(old);
    dirty = 1;
}

/* Delete character at column position in a line */
void aria_editor_delete_char(int row, int col) {
    if (row < 0 || row >= line_count) return;
    int len = (int)strlen(lines[row]);
    if (col < 0 || col >= len) return;
    memmove(lines[row] + col, lines[row] + col + 1,
            (size_t)(len - col));  /* includes null terminator */
    dirty = 1;
}

/* Split line at column position (for Enter key).
 * Characters from col onward move to a new line below. */
void aria_editor_split_line(int row, int col) {
    if (row < 0 || row >= line_count) return;
    int len = (int)strlen(lines[row]);
    if (col < 0) col = 0;
    if (col > len) col = len;

    /* Create new line from the tail */
    char *tail = dup_str(lines[row] + col);

    /* Truncate current line */
    lines[row][col] = '\0';

    /* Insert new line below */
    aria_editor_insert_line(row + 1, tail);
    free(tail);
}

/* Join line with the one below it (for Backspace at col 0 or Delete at EOL) */
void aria_editor_join_lines(int row) {
    if (row < 0 || row >= line_count - 1) return;
    int len1 = (int)strlen(lines[row]);
    int len2 = (int)strlen(lines[row + 1]);
    char *merged = malloc((size_t)(len1 + len2 + 1));
    if (!merged) return;
    memcpy(merged, lines[row], (size_t)len1);
    memcpy(merged + len1, lines[row + 1], (size_t)(len2 + 1));
    free(lines[row]);
    lines[row] = merged;
    /* Remove the line below */
    free(lines[row + 1]);
    memmove(&lines[row + 1], &lines[row + 2],
            sizeof(char*) * (size_t)(line_count - row - 2));
    line_count--;
    dirty = 1;
}

/* Join all lines into single string with '\n' between them.
 * Caller must free() the result. */
const char *aria_editor_to_string(void) {
    size_t total = 0;
    for (int i = 0; i < line_count; i++)
        total += strlen(lines[i]) + 1;  /* +1 for \n */

    char *buf = malloc(total + 1);
    if (!buf) return "";
    char *p = buf;
    for (int i = 0; i < line_count; i++) {
        size_t len = strlen(lines[i]);
        memcpy(p, lines[i], len);
        p += len;
        *p++ = '\n';
    }
    *p = '\0';
    return buf;
}

/* Return 1 if buffer has been modified since last load/save */
int aria_editor_is_dirty(void) {
    return dirty;
}

/* Clear the dirty flag (call after save) */
void aria_editor_clear_dirty(void) {
    dirty = 0;
}

/* Find text in buffer starting from (row, col).
 * Returns packed int64: (row << 32) | col, or -1 if not found.
 * Search wraps around. */
long long aria_editor_find(const char *needle, int from_row, int from_col) {
    if (!needle || *needle == '\0') return -1;
    int nlen = (int)strlen(needle);

    for (int i = 0; i < line_count; i++) {
        int r = (from_row + i) % line_count;
        int start_col = (i == 0) ? from_col : 0;
        const char *line = lines[r];
        int llen = (int)strlen(line);
        for (int c = start_col; c <= llen - nlen; c++) {
            if (strncmp(line + c, needle, (size_t)nlen) == 0) {
                return ((long long)r << 32) | (long long)c;
            }
        }
    }
    return -1;
}

/* Extract row from packed find result */
int aria_editor_find_row(long long packed) {
    return (int)(packed >> 32);
}

/* Extract col from packed find result */
int aria_editor_find_col(long long packed) {
    return (int)(packed & 0xFFFFFFFF);
}
