/*  aria_sqlite_shim.c — flat-parameter bridge between Aria FFI and libsqlite3
 *
 *  Manages up to 16 open database connections and 16 prepared statements.
 *  Results are captured in an internal row buffer for column-by-column access.
 *
 *  Build:
 *    cc -O2 -shared -fPIC -Wall -o libaria_sqlite_shim.so aria_sqlite_shim.c -lsqlite3
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sqlite3.h>

/* ── connection pool ─────────────────────────────────────────────────── */

#define MAX_CONNECTIONS 16
#define MAX_STATEMENTS  16
#define MAX_BIND_PARAMS 32

static sqlite3 *g_dbs[MAX_CONNECTIONS] = {0};
static sqlite3_stmt *g_stmts[MAX_STATEMENTS] = {0};
static int32_t g_stmt_db[MAX_STATEMENTS] = {0};  /* which db owns each stmt */

/* ── last error state ────────────────────────────────────────────────── */

static char g_last_error[1024] = {0};

static void set_error(const char *msg) {
    if (msg) {
        strncpy(g_last_error, msg, sizeof(g_last_error) - 1);
        g_last_error[sizeof(g_last_error) - 1] = '\0';
    } else {
        g_last_error[0] = '\0';
    }
}

static void set_error_from_db(int32_t db_id) {
    if (db_id >= 0 && db_id < MAX_CONNECTIONS && g_dbs[db_id]) {
        set_error(sqlite3_errmsg(g_dbs[db_id]));
    } else {
        set_error("invalid database handle");
    }
}

/* ── result row buffer ───────────────────────────────────────────────── */

#define MAX_COLUMNS 64

static char  *g_row_values[MAX_COLUMNS] = {0};
static int    g_row_types[MAX_COLUMNS]  = {0};
static int    g_col_count               = 0;

static void clear_row(void) {
    for (int i = 0; i < MAX_COLUMNS; i++) {
        free(g_row_values[i]);
        g_row_values[i] = NULL;
        g_row_types[i]  = 0;
    }
    g_col_count = 0;
}

static void capture_row(sqlite3_stmt *stmt) {
    clear_row();
    g_col_count = sqlite3_column_count(stmt);
    if (g_col_count > MAX_COLUMNS) g_col_count = MAX_COLUMNS;
    for (int i = 0; i < g_col_count; i++) {
        g_row_types[i] = sqlite3_column_type(stmt, i);
        const char *text = (const char *)sqlite3_column_text(stmt, i);
        if (text) {
            g_row_values[i] = strdup(text);
        } else {
            g_row_values[i] = NULL;
        }
    }
}

/* ── open / close ────────────────────────────────────────────────────── */

int32_t aria_sqlite_open(const char *path) {
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (!g_dbs[i]) {
            int rc = sqlite3_open(path, &g_dbs[i]);
            if (rc != SQLITE_OK) {
                set_error(sqlite3_errmsg(g_dbs[i]));
                sqlite3_close(g_dbs[i]);
                g_dbs[i] = NULL;
                return -1;
            }
            /* Enable WAL mode for better concurrent access */
            sqlite3_exec(g_dbs[i], "PRAGMA journal_mode=WAL;", NULL, NULL, NULL);
            set_error("");
            return (int32_t)i;
        }
    }
    set_error("too many open databases (max 16)");
    return -1;
}

int32_t aria_sqlite_disconnect(int32_t db_id) {
    if (db_id < 0 || db_id >= MAX_CONNECTIONS || !g_dbs[db_id]) {
        set_error("invalid database handle");
        return 0;
    }
    /* finalize any statements belonging to this db */
    for (int i = 0; i < MAX_STATEMENTS; i++) {
        if (g_stmts[i] && g_stmt_db[i] == db_id) {
            sqlite3_finalize(g_stmts[i]);
            g_stmts[i] = NULL;
        }
    }
    sqlite3_close(g_dbs[db_id]);
    g_dbs[db_id] = NULL;
    set_error("");
    return 1;
}

/* ── execute (no result set) ─────────────────────────────────────────── */

int32_t aria_sqlite_exec(int32_t db_id, const char *sql) {
    if (db_id < 0 || db_id >= MAX_CONNECTIONS || !g_dbs[db_id]) {
        set_error("invalid database handle");
        return 0;
    }
    char *errmsg = NULL;
    int rc = sqlite3_exec(g_dbs[db_id], sql, NULL, NULL, &errmsg);
    if (rc != SQLITE_OK) {
        set_error(errmsg ? errmsg : sqlite3_errmsg(g_dbs[db_id]));
        sqlite3_free(errmsg);
        return 0;
    }
    sqlite3_free(errmsg);
    set_error("");
    return 1;
}

/* ── prepared statements ─────────────────────────────────────────────── */

int32_t aria_sqlite_prepare(int32_t db_id, const char *sql) {
    if (db_id < 0 || db_id >= MAX_CONNECTIONS || !g_dbs[db_id]) {
        set_error("invalid database handle");
        return -1;
    }
    for (int i = 0; i < MAX_STATEMENTS; i++) {
        if (!g_stmts[i]) {
            int rc = sqlite3_prepare_v2(g_dbs[db_id], sql, -1, &g_stmts[i], NULL);
            if (rc != SQLITE_OK) {
                set_error_from_db(db_id);
                g_stmts[i] = NULL;
                return -1;
            }
            g_stmt_db[i] = db_id;
            set_error("");
            return (int32_t)i;
        }
    }
    set_error("too many prepared statements (max 16)");
    return -1;
}

int32_t aria_sqlite_finalize(int32_t stmt_id) {
    if (stmt_id < 0 || stmt_id >= MAX_STATEMENTS || !g_stmts[stmt_id]) {
        set_error("invalid statement handle");
        return 0;
    }
    sqlite3_finalize(g_stmts[stmt_id]);
    g_stmts[stmt_id] = NULL;
    set_error("");
    return 1;
}

int32_t aria_sqlite_reset(int32_t stmt_id) {
    if (stmt_id < 0 || stmt_id >= MAX_STATEMENTS || !g_stmts[stmt_id]) {
        set_error("invalid statement handle");
        return 0;
    }
    sqlite3_reset(g_stmts[stmt_id]);
    sqlite3_clear_bindings(g_stmts[stmt_id]);
    set_error("");
    return 1;
}

/* ── bind parameters (parameterized queries) ─────────────────────────── */

int32_t aria_sqlite_bind_int(int32_t stmt_id, int32_t idx, int64_t val) {
    if (stmt_id < 0 || stmt_id >= MAX_STATEMENTS || !g_stmts[stmt_id]) {
        set_error("invalid statement handle");
        return 0;
    }
    int rc = sqlite3_bind_int64(g_stmts[stmt_id], idx, val);
    if (rc != SQLITE_OK) {
        set_error_from_db(g_stmt_db[stmt_id]);
        return 0;
    }
    set_error("");
    return 1;
}

int32_t aria_sqlite_bind_double(int32_t stmt_id, int32_t idx, double val) {
    if (stmt_id < 0 || stmt_id >= MAX_STATEMENTS || !g_stmts[stmt_id]) {
        set_error("invalid statement handle");
        return 0;
    }
    int rc = sqlite3_bind_double(g_stmts[stmt_id], idx, val);
    if (rc != SQLITE_OK) {
        set_error_from_db(g_stmt_db[stmt_id]);
        return 0;
    }
    set_error("");
    return 1;
}

int32_t aria_sqlite_bind_text(int32_t stmt_id, int32_t idx, const char *val) {
    if (stmt_id < 0 || stmt_id >= MAX_STATEMENTS || !g_stmts[stmt_id]) {
        set_error("invalid statement handle");
        return 0;
    }
    int rc = sqlite3_bind_text(g_stmts[stmt_id], idx, val, -1, SQLITE_TRANSIENT);
    if (rc != SQLITE_OK) {
        set_error_from_db(g_stmt_db[stmt_id]);
        return 0;
    }
    set_error("");
    return 1;
}

int32_t aria_sqlite_bind_null(int32_t stmt_id, int32_t idx) {
    if (stmt_id < 0 || stmt_id >= MAX_STATEMENTS || !g_stmts[stmt_id]) {
        set_error("invalid statement handle");
        return 0;
    }
    int rc = sqlite3_bind_null(g_stmts[stmt_id], idx);
    if (rc != SQLITE_OK) {
        set_error_from_db(g_stmt_db[stmt_id]);
        return 0;
    }
    set_error("");
    return 1;
}

/* ── step / fetch ────────────────────────────────────────────────────── */

int32_t aria_sqlite_step(int32_t stmt_id) {
    if (stmt_id < 0 || stmt_id >= MAX_STATEMENTS || !g_stmts[stmt_id]) {
        set_error("invalid statement handle");
        return -1;
    }
    int rc = sqlite3_step(g_stmts[stmt_id]);
    if (rc == SQLITE_ROW) {
        capture_row(g_stmts[stmt_id]);
        set_error("");
        return 1;   /* row available */
    }
    if (rc == SQLITE_DONE) {
        clear_row();
        set_error("");
        return 0;   /* no more rows */
    }
    /* error */
    set_error_from_db(g_stmt_db[stmt_id]);
    return -1;
}

/* ── column accessors (from last step) ───────────────────────────────── */

int32_t aria_sqlite_column_count(void) {
    return (int32_t)g_col_count;
}

int32_t aria_sqlite_column_type(int32_t col) {
    if (col < 0 || col >= g_col_count) return 0;
    return (int32_t)g_row_types[col];
}

const char *aria_sqlite_column_name(int32_t stmt_id, int32_t col) {
    if (stmt_id < 0 || stmt_id >= MAX_STATEMENTS || !g_stmts[stmt_id]) return "";
    const char *name = sqlite3_column_name(g_stmts[stmt_id], col);
    return name ? name : "";
}

const char *aria_sqlite_column_text(int32_t col) {
    if (col < 0 || col >= g_col_count || !g_row_values[col]) return "";
    return g_row_values[col];
}

int64_t aria_sqlite_column_int(int32_t col) {
    if (col < 0 || col >= g_col_count || !g_row_values[col]) return 0;
    return (int64_t)strtoll(g_row_values[col], NULL, 10);
}

double aria_sqlite_column_double(int32_t col) {
    if (col < 0 || col >= g_col_count || !g_row_values[col]) return 0.0;
    return strtod(g_row_values[col], NULL);
}

int32_t aria_sqlite_column_is_null(int32_t col) {
    if (col < 0 || col >= g_col_count) return 1;
    return (g_row_types[col] == SQLITE_NULL) ? 1 : 0;
}

/* ── transaction helpers ─────────────────────────────────────────────── */

int32_t aria_sqlite_begin(int32_t db_id) {
    return aria_sqlite_exec(db_id, "BEGIN TRANSACTION;");
}

int32_t aria_sqlite_commit(int32_t db_id) {
    return aria_sqlite_exec(db_id, "COMMIT;");
}

int32_t aria_sqlite_rollback(int32_t db_id) {
    return aria_sqlite_exec(db_id, "ROLLBACK;");
}

/* ── handle retrieval (work around Aria move semantics) ──────────────── */

int32_t aria_sqlite_last_db(void) {
    for (int i = MAX_CONNECTIONS - 1; i >= 0; i--) {
        if (g_dbs[i]) return (int32_t)i;
    }
    return -1;
}

int32_t aria_sqlite_last_stmt(void) {
    for (int i = MAX_STATEMENTS - 1; i >= 0; i--) {
        if (g_stmts[i]) return (int32_t)i;
    }
    return -1;
}

/* ── utility ─────────────────────────────────────────────────────────── */

int64_t aria_sqlite_last_insert_id(int32_t db_id) {
    if (db_id < 0 || db_id >= MAX_CONNECTIONS || !g_dbs[db_id]) return -1;
    return (int64_t)sqlite3_last_insert_rowid(g_dbs[db_id]);
}

int32_t aria_sqlite_changes(int32_t db_id) {
    if (db_id < 0 || db_id >= MAX_CONNECTIONS || !g_dbs[db_id]) return 0;
    return (int32_t)sqlite3_changes(g_dbs[db_id]);
}

const char *aria_sqlite_error(void) {
    return g_last_error;
}

const char *aria_sqlite_version(void) {
    return sqlite3_libversion();
}

/* ── test helpers ────────────────────────────────────────────────────── */

static int g_test_passed = 0;
static int g_test_failed = 0;

void aria_sqlite_assert_int_eq(int32_t actual, int32_t expected, const char *msg) {
    if (actual == expected) {
        g_test_passed++;
        printf("  [PASS] %s\n", msg);
    } else {
        g_test_failed++;
        printf("  [FAIL] %s (got %d, expected %d)\n", msg, actual, expected);
    }
}

void aria_sqlite_assert_int64_eq(int64_t actual, int64_t expected, const char *msg) {
    if (actual == expected) {
        g_test_passed++;
        printf("  [PASS] %s\n", msg);
    } else {
        g_test_failed++;
        printf("  [FAIL] %s (got %ld, expected %ld)\n", msg, (long)actual, (long)expected);
    }
}

void aria_sqlite_assert_str_eq(const char *actual, const char *expected, const char *msg) {
    if (actual && expected && strcmp(actual, expected) == 0) {
        g_test_passed++;
        printf("  [PASS] %s\n", msg);
    } else {
        g_test_failed++;
        printf("  [FAIL] %s (got \"%s\", expected \"%s\")\n",
               msg, actual ? actual : "(null)", expected ? expected : "(null)");
    }
}

void aria_sqlite_assert_true(int8_t val, const char *msg) {
    if (val) {
        g_test_passed++;
        printf("  [PASS] %s\n", msg);
    } else {
        g_test_failed++;
        printf("  [FAIL] %s (expected true, got false)\n", msg);
    }
}

/* column assertion helpers (avoid returning strings to Aria) */

void aria_sqlite_assert_col_text_eq(int32_t col, const char *expected, const char *msg) {
    const char *actual = aria_sqlite_column_text(col);
    aria_sqlite_assert_str_eq(actual, expected, msg);
}

void aria_sqlite_assert_col_int_eq(int32_t col, int64_t expected, const char *msg) {
    int64_t actual = aria_sqlite_column_int(col);
    aria_sqlite_assert_int64_eq(actual, expected, msg);
}

void aria_sqlite_assert_col_is_null(int32_t col, const char *msg) {
    int32_t actual = aria_sqlite_column_is_null(col);
    aria_sqlite_assert_int_eq(actual, 1, msg);
}

void aria_sqlite_assert_version_ok(const char *msg) {
    const char *ver = aria_sqlite_version();
    if (ver && strlen(ver) > 0) {
        g_test_passed++;
        printf("  [PASS] %s (version: %s)\n", msg, ver);
    } else {
        g_test_failed++;
        printf("  [FAIL] %s (no version string)\n", msg);
    }
}

void aria_sqlite_test_summary(void) {
    printf("\n=== aria-sqlite test summary ===\n");
    printf("passed=%d failed=%d total=%d\n",
           g_test_passed, g_test_failed, g_test_passed + g_test_failed);
    if (g_test_failed > 0) {
        printf("RESULT: SOME TESTS FAILED\n");
    } else {
        printf("RESULT: ALL TESTS PASSED\n");
    }
}
