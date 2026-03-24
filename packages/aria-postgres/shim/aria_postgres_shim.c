/*
 * aria_postgres_shim.c — C shim for Aria ↔ libpq (PostgreSQL)
 *
 * Design mirrors aria_sqlite_shim.c:
 *   - Connection pool (16 slots)
 *   - Result buffering (current PGresult + row cursor)
 *   - Handle helpers: last_conn() for move-semantics workaround
 *   - "disconnect" instead of "close" (compiler bug workaround)
 *   - C-side string assertion helpers (string return corruption workaround)
 *
 * Build:
 *   cc -O2 -shared -fPIC -Wall -o libaria_postgres_shim.so aria_postgres_shim.c -lpq
 */

#include <libpq-fe.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* ── pools ────────────────────────────────────────────────────── */

#define MAX_CONNS 16
#define MAX_PARAMS 32

static PGconn   *g_conns[MAX_CONNS];
static PGresult  *g_results[MAX_CONNS];  /* current result per connection */
static int        g_row_cursor[MAX_CONNS]; /* current row index per connection */

/* ── connection management ────────────────────────────────────── */

int32_t aria_postgres_connect(const char *conninfo) {
    for (int i = 0; i < MAX_CONNS; i++) {
        if (!g_conns[i]) {
            g_conns[i] = PQconnectdb(conninfo);
            if (PQstatus(g_conns[i]) != CONNECTION_OK) {
                fprintf(stderr, "[aria-postgres] connect failed: %s\n",
                        PQerrorMessage(g_conns[i]));
                PQfinish(g_conns[i]);
                g_conns[i] = NULL;
                return -1;
            }
            return i;
        }
    }
    fprintf(stderr, "[aria-postgres] connection pool full (%d)\n", MAX_CONNS);
    return -1;
}

int32_t aria_postgres_disconnect(int32_t conn_id) {
    if (conn_id < 0 || conn_id >= MAX_CONNS || !g_conns[conn_id]) return 0;
    if (g_results[conn_id]) {
        PQclear(g_results[conn_id]);
        g_results[conn_id] = NULL;
    }
    PQfinish(g_conns[conn_id]);
    g_conns[conn_id] = NULL;
    g_row_cursor[conn_id] = 0;
    return 1;
}

int32_t aria_postgres_last_conn(void) {
    for (int i = MAX_CONNS - 1; i >= 0; i--) {
        if (g_conns[i]) return i;
    }
    return -1;
}

/* ── query execution ──────────────────────────────────────────── */

/* Clear any previous result for this connection */
static void clear_result(int32_t conn_id) {
    if (conn_id >= 0 && conn_id < MAX_CONNS && g_results[conn_id]) {
        PQclear(g_results[conn_id]);
        g_results[conn_id] = NULL;
        g_row_cursor[conn_id] = 0;
    }
}

/* Execute a simple SQL statement (no params, no result set needed) */
int32_t aria_postgres_exec(int32_t conn_id, const char *sql) {
    if (conn_id < 0 || conn_id >= MAX_CONNS || !g_conns[conn_id]) return 0;
    clear_result(conn_id);
    PGresult *res = PQexec(g_conns[conn_id], sql);
    ExecStatusType st = PQresultStatus(res);
    if (st != PGRES_COMMAND_OK && st != PGRES_TUPLES_OK) {
        fprintf(stderr, "[aria-postgres] exec error: %s\n",
                PQerrorMessage(g_conns[conn_id]));
        PQclear(res);
        return 0;
    }
    PQclear(res);
    return 1;
}

/* Execute a parameterized query — stores result for row iteration.
 * params is a NULL-terminated array of C strings.
 * For $1, $2, ... placeholders in sql.
 * nparams is the count of parameters.
 */

/* Param staging area — Aria can't pass arrays, so we stage params one at a time */
static const char *g_params[MAX_PARAMS];
static int g_param_count = 0;

int32_t aria_postgres_param_clear(void) {
    for (int i = 0; i < g_param_count; i++) {
        /* params were strdup'd */
        if (g_params[i]) {
            free((void*)g_params[i]);
            g_params[i] = NULL;
        }
    }
    g_param_count = 0;
    return 1;
}

int32_t aria_postgres_param_add_text(const char *val) {
    if (g_param_count >= MAX_PARAMS) return 0;
    g_params[g_param_count++] = strdup(val);
    return 1;
}

int32_t aria_postgres_param_add_int(int64_t val) {
    if (g_param_count >= MAX_PARAMS) return 0;
    char buf[32];
    snprintf(buf, sizeof(buf), "%ld", (long)val);
    g_params[g_param_count++] = strdup(buf);
    return 1;
}

int32_t aria_postgres_param_add_double(double val) {
    if (g_param_count >= MAX_PARAMS) return 0;
    char buf[64];
    snprintf(buf, sizeof(buf), "%.17g", val);
    g_params[g_param_count++] = strdup(buf);
    return 1;
}

int32_t aria_postgres_param_add_null(void) {
    if (g_param_count >= MAX_PARAMS) return 0;
    g_params[g_param_count++] = NULL; /* NULL pointer = SQL NULL */
    return 1;
}

/* Execute parameterized query and store result */
int32_t aria_postgres_query(int32_t conn_id, const char *sql) {
    if (conn_id < 0 || conn_id >= MAX_CONNS || !g_conns[conn_id]) return 0;
    clear_result(conn_id);

    PGresult *res = PQexecParams(
        g_conns[conn_id], sql,
        g_param_count,
        NULL,           /* paramTypes — let server infer */
        g_params,       /* paramValues */
        NULL,           /* paramLengths — text mode */
        NULL,           /* paramFormats — text mode */
        0               /* resultFormat — text */
    );

    /* Clear staged params after use */
    aria_postgres_param_clear();

    ExecStatusType st = PQresultStatus(res);
    if (st != PGRES_COMMAND_OK && st != PGRES_TUPLES_OK) {
        fprintf(stderr, "[aria-postgres] query error: %s\n",
                PQerrorMessage(g_conns[conn_id]));
        PQclear(res);
        return 0;
    }

    g_results[conn_id] = res;
    g_row_cursor[conn_id] = -1; /* before first row */
    return 1;
}

/* Execute parameterized non-SELECT (INSERT/UPDATE/DELETE) and store result */
int32_t aria_postgres_execute(int32_t conn_id, const char *sql) {
    /* Same as query but semantically for non-SELECT */
    return aria_postgres_query(conn_id, sql);
}

/* ── result navigation ────────────────────────────────────────── */

/* Advance to next row. Returns 1 if row available, 0 if no more rows. */
int32_t aria_postgres_next_row(int32_t conn_id) {
    if (conn_id < 0 || conn_id >= MAX_CONNS || !g_results[conn_id]) return 0;
    int nrows = PQntuples(g_results[conn_id]);
    g_row_cursor[conn_id]++;
    if (g_row_cursor[conn_id] < nrows) return 1;
    return 0;
}

/* Number of rows in result */
int32_t aria_postgres_row_count(int32_t conn_id) {
    if (conn_id < 0 || conn_id >= MAX_CONNS || !g_results[conn_id]) return 0;
    return PQntuples(g_results[conn_id]);
}

/* Number of columns in result */
int32_t aria_postgres_column_count(int32_t conn_id) {
    if (conn_id < 0 || conn_id >= MAX_CONNS || !g_results[conn_id]) return 0;
    return PQnfields(g_results[conn_id]);
}

/* Get column value as string (returns pointer to internal buffer) */
const char *aria_postgres_column_text(int32_t conn_id, int32_t col) {
    if (conn_id < 0 || conn_id >= MAX_CONNS || !g_results[conn_id]) return "";
    int row = g_row_cursor[conn_id];
    if (row < 0 || row >= PQntuples(g_results[conn_id])) return "";
    if (col < 0 || col >= PQnfields(g_results[conn_id])) return "";
    if (PQgetisnull(g_results[conn_id], row, col)) return "";
    return PQgetvalue(g_results[conn_id], row, col);
}

/* Get column value as int64 */
int64_t aria_postgres_column_int(int32_t conn_id, int32_t col) {
    const char *val = aria_postgres_column_text(conn_id, col);
    if (!val || !*val) return 0;
    return strtol(val, NULL, 10);
}

/* Get column value as double */
double aria_postgres_column_double(int32_t conn_id, int32_t col) {
    const char *val = aria_postgres_column_text(conn_id, col);
    if (!val || !*val) return 0.0;
    return strtod(val, NULL);
}

/* Check if column is NULL */
int32_t aria_postgres_column_is_null(int32_t conn_id, int32_t col) {
    if (conn_id < 0 || conn_id >= MAX_CONNS || !g_results[conn_id]) return 1;
    int row = g_row_cursor[conn_id];
    if (row < 0 || row >= PQntuples(g_results[conn_id])) return 1;
    if (col < 0 || col >= PQnfields(g_results[conn_id])) return 1;
    return PQgetisnull(g_results[conn_id], row, col) ? 1 : 0;
}

/* Clear stored result */
int32_t aria_postgres_clear_result(int32_t conn_id) {
    clear_result(conn_id);
    return 1;
}

/* ── affected rows / last insert id ──────────────────────────── */

/* Number of rows affected by last INSERT/UPDATE/DELETE */
int64_t aria_postgres_affected_rows(int32_t conn_id) {
    if (conn_id < 0 || conn_id >= MAX_CONNS || !g_results[conn_id]) return 0;
    const char *val = PQcmdTuples(g_results[conn_id]);
    if (!val || !*val) return 0;
    return strtol(val, NULL, 10);
}

/* ── transactions ─────────────────────────────────────────────── */

int32_t aria_postgres_begin(int32_t conn_id) {
    return aria_postgres_exec(conn_id, "BEGIN");
}

int32_t aria_postgres_commit(int32_t conn_id) {
    return aria_postgres_exec(conn_id, "COMMIT");
}

int32_t aria_postgres_rollback(int32_t conn_id) {
    return aria_postgres_exec(conn_id, "ROLLBACK");
}

/* ── utility ──────────────────────────────────────────────────── */

const char *aria_postgres_error(int32_t conn_id) {
    if (conn_id < 0 || conn_id >= MAX_CONNS || !g_conns[conn_id]) return "invalid connection";
    return PQerrorMessage(g_conns[conn_id]);
}

int32_t aria_postgres_status(int32_t conn_id) {
    if (conn_id < 0 || conn_id >= MAX_CONNS || !g_conns[conn_id]) return 0;
    return (PQstatus(g_conns[conn_id]) == CONNECTION_OK) ? 1 : 0;
}

const char *aria_postgres_server_version(int32_t conn_id) {
    static char buf[32];
    if (conn_id < 0 || conn_id >= MAX_CONNS || !g_conns[conn_id]) return "unknown";
    int ver = PQserverVersion(g_conns[conn_id]);
    snprintf(buf, sizeof(buf), "%d.%d.%d", ver / 10000, (ver / 100) % 100, ver % 100);
    return buf;
}

/* ── LISTEN / NOTIFY ──────────────────────────────────────────── */

int32_t aria_postgres_listen(int32_t conn_id, const char *channel) {
    if (conn_id < 0 || conn_id >= MAX_CONNS || !g_conns[conn_id]) return 0;
    /* Build LISTEN command safely — channel is an identifier, not a value */
    char sql[256];
    /* Validate channel name: only alphanumeric and underscore */
    for (const char *p = channel; *p; p++) {
        if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
              (*p >= '0' && *p <= '9') || *p == '_')) {
            fprintf(stderr, "[aria-postgres] invalid channel name\n");
            return 0;
        }
    }
    snprintf(sql, sizeof(sql), "LISTEN %s", channel);
    return aria_postgres_exec(conn_id, sql);
}

int32_t aria_postgres_notify(int32_t conn_id, const char *channel, const char *payload) {
    if (conn_id < 0 || conn_id >= MAX_CONNS || !g_conns[conn_id]) return 0;
    /* Use pg_notify() function for safe payload handling */
    aria_postgres_param_clear();
    aria_postgres_param_add_text(channel);
    aria_postgres_param_add_text(payload);
    PGresult *res = PQexecParams(
        g_conns[conn_id],
        "SELECT pg_notify($1, $2)",
        2, NULL,
        g_params, NULL, NULL, 0
    );
    aria_postgres_param_clear();
    ExecStatusType st = PQresultStatus(res);
    PQclear(res);
    return (st == PGRES_TUPLES_OK || st == PGRES_COMMAND_OK) ? 1 : 0;
}

/* ── test assertion helpers (C-side, avoids string return bug) ── */

static int g_test_pass = 0;
static int g_test_fail = 0;

void aria_postgres_assert_int_eq(int64_t actual, int64_t expected, const char *msg) {
    if (actual == expected) {
        printf("  PASS: %s\n", msg);
        g_test_pass++;
    } else {
        printf("  FAIL: %s — expected %ld, got %ld\n", msg, (long)expected, (long)actual);
        g_test_fail++;
    }
}

void aria_postgres_assert_true(int8_t val, const char *msg) {
    if (val) {
        printf("  PASS: %s\n", msg);
        g_test_pass++;
    } else {
        printf("  FAIL: %s — expected true\n", msg);
        g_test_fail++;
    }
}

/* Assert column text value equals expected (comparison done in C) */
void aria_postgres_assert_col_text_eq(int32_t conn_id, int32_t col,
                                       const char *expected, const char *msg) {
    const char *actual = aria_postgres_column_text(conn_id, col);
    if (strcmp(actual, expected) == 0) {
        printf("  PASS: %s (value=\"%s\")\n", msg, actual);
        g_test_pass++;
    } else {
        printf("  FAIL: %s — expected \"%s\", got \"%s\"\n", msg, expected, actual);
        g_test_fail++;
    }
}

/* Assert column int value equals expected */
void aria_postgres_assert_col_int_eq(int32_t conn_id, int32_t col,
                                      int64_t expected, const char *msg) {
    int64_t actual = aria_postgres_column_int(conn_id, col);
    if (actual == expected) {
        printf("  PASS: %s (value=%ld)\n", msg, (long)actual);
        g_test_pass++;
    } else {
        printf("  FAIL: %s — expected %ld, got %ld\n", msg, (long)expected, (long)actual);
        g_test_fail++;
    }
}

/* Assert column is NULL */
void aria_postgres_assert_col_is_null(int32_t conn_id, int32_t col, const char *msg) {
    if (aria_postgres_column_is_null(conn_id, col)) {
        printf("  PASS: %s\n", msg);
        g_test_pass++;
    } else {
        printf("  FAIL: %s — expected NULL\n", msg);
        g_test_fail++;
    }
}

/* Assert server version is available */
void aria_postgres_assert_version_ok(int32_t conn_id, const char *msg) {
    const char *ver = aria_postgres_server_version(conn_id);
    if (ver && *ver && strcmp(ver, "unknown") != 0) {
        printf("  PASS: %s (version=%s)\n", msg, ver);
        g_test_pass++;
    } else {
        printf("  FAIL: %s — could not get server version\n", msg);
        g_test_fail++;
    }
}

/* Assert affected rows equals expected */
void aria_postgres_assert_affected_eq(int32_t conn_id, int64_t expected, const char *msg) {
    int64_t actual = aria_postgres_affected_rows(conn_id);
    if (actual == expected) {
        printf("  PASS: %s (affected=%ld)\n", msg, (long)actual);
        g_test_pass++;
    } else {
        printf("  FAIL: %s — expected %ld affected, got %ld\n", msg, (long)expected, (long)actual);
        g_test_fail++;
    }
}

void aria_postgres_test_summary(void) {
    int total = g_test_pass + g_test_fail;
    printf("\n========================================\n");
    printf("passed=%d failed=%d total=%d\n", g_test_pass, g_test_fail, total);
    if (g_test_fail == 0)
        printf("RESULT: ALL TESTS PASSED\n");
    else
        printf("RESULT: SOME TESTS FAILED\n");
    printf("========================================\n");
}
