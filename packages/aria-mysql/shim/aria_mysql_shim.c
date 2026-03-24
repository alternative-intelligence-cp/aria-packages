/*
 * aria_mysql_shim.c — C shim for Aria ↔ libmysqlclient (MySQL)
 *
 * Design mirrors aria_postgres_shim.c:
 *   - Connection pool (16 slots)
 *   - Simple query execution + parameterized queries via mysql_stmt
 *   - Parameter staging (Aria can't pass arrays)
 *   - Result buffering with row cursor
 *   - Handle helpers: last_conn() for move-semantics workaround
 *   - "disconnect" instead of "close" (compiler bug workaround)
 *   - C-side string assertion helpers (string return corruption workaround)
 *
 * Build:
 *   cc -O2 -shared -fPIC -Wall -o libaria_mysql_shim.so aria_mysql_shim.c \
 *      $(mysql_config --cflags --libs) 2>/dev/null || \
 *   cc -O2 -shared -fPIC -Wall -I/usr/include/mysql -o libaria_mysql_shim.so \
 *      aria_mysql_shim.c -lmysqlclient
 */

#include <mysql/mysql.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* ── pools ────────────────────────────────────────────────────── */

#define MAX_CONNS   16
#define MAX_PARAMS  32
#define MAX_COLS    64
#define COL_BUF_SZ  4096

static MYSQL       *g_conns[MAX_CONNS];
static MYSQL_RES   *g_results[MAX_CONNS];
static MYSQL_ROW    g_current_row[MAX_CONNS];
static unsigned int g_num_fields[MAX_CONNS];
static int64_t     g_last_affected[MAX_CONNS];

/* ── param staging ────────────────────────────────────────────── */

typedef enum { P_TEXT, P_INT, P_DOUBLE, P_NULL } ParamType;

typedef struct {
    ParamType type;
    char      text[COL_BUF_SZ];
    int64_t   ival;
    double    dval;
} StagedParam;

static StagedParam g_params[MAX_PARAMS];
static int g_param_count = 0;

/* ── connection management ────────────────────────────────────── */

int32_t aria_mysql_connect(const char *host, const char *user,
                           const char *passwd, const char *db,
                           int32_t port) {
    for (int i = 0; i < MAX_CONNS; i++) {
        if (!g_conns[i]) {
            g_conns[i] = mysql_init(NULL);
            if (!g_conns[i]) {
                fprintf(stderr, "[aria-mysql] mysql_init failed\n");
                return -1;
            }
            if (!mysql_real_connect(g_conns[i],
                    host, user, passwd, db,
                    (unsigned int)port, NULL, 0)) {
                fprintf(stderr, "[aria-mysql] connect failed: %s\n",
                        mysql_error(g_conns[i]));
                mysql_close(g_conns[i]);
                g_conns[i] = NULL;
                return -1;
            }
            /* Set UTF-8 */
            mysql_set_character_set(g_conns[i], "utf8mb4");
            return i;
        }
    }
    fprintf(stderr, "[aria-mysql] connection pool full (%d)\n", MAX_CONNS);
    return -1;
}

int32_t aria_mysql_disconnect(int32_t conn_id) {
    if (conn_id < 0 || conn_id >= MAX_CONNS || !g_conns[conn_id]) return 0;
    if (g_results[conn_id]) {
        mysql_free_result(g_results[conn_id]);
        g_results[conn_id] = NULL;
    }
    mysql_close(g_conns[conn_id]);
    g_conns[conn_id] = NULL;
    g_current_row[conn_id] = NULL;
    g_num_fields[conn_id] = 0;
    return 1;
}

int32_t aria_mysql_last_conn(void) {
    for (int i = MAX_CONNS - 1; i >= 0; i--) {
        if (g_conns[i]) return i;
    }
    return -1;
}

/* ── simple query execution ───────────────────────────────────── */

static void clear_result(int32_t conn_id) {
    if (conn_id >= 0 && conn_id < MAX_CONNS && g_results[conn_id]) {
        mysql_free_result(g_results[conn_id]);
        g_results[conn_id] = NULL;
        g_current_row[conn_id] = NULL;
        g_num_fields[conn_id] = 0;
    }
}

int32_t aria_mysql_exec(int32_t conn_id, const char *sql) {
    if (conn_id < 0 || conn_id >= MAX_CONNS || !g_conns[conn_id]) return 0;
    clear_result(conn_id);
    if (mysql_query(g_conns[conn_id], sql) != 0) {
        fprintf(stderr, "[aria-mysql] exec error: %s\n",
                mysql_error(g_conns[conn_id]));
        return 0;
    }
    /* Consume result if any (prevents "Commands out of sync") */
    MYSQL_RES *res = mysql_store_result(g_conns[conn_id]);
    if (res) {
        mysql_free_result(res);
    }
    return 1;
}

/* ── param staging ────────────────────────────────────────────── */

int32_t aria_mysql_param_clear(void) {
    g_param_count = 0;
    return 1;
}

int32_t aria_mysql_param_add_text(const char *val) {
    if (g_param_count >= MAX_PARAMS) return 0;
    g_params[g_param_count].type = P_TEXT;
    strncpy(g_params[g_param_count].text, val, COL_BUF_SZ - 1);
    g_params[g_param_count].text[COL_BUF_SZ - 1] = '\0';
    g_param_count++;
    return 1;
}

int32_t aria_mysql_param_add_int(int64_t val) {
    if (g_param_count >= MAX_PARAMS) return 0;
    g_params[g_param_count].type = P_INT;
    g_params[g_param_count].ival = val;
    g_param_count++;
    return 1;
}

int32_t aria_mysql_param_add_double(double val) {
    if (g_param_count >= MAX_PARAMS) return 0;
    g_params[g_param_count].type = P_DOUBLE;
    g_params[g_param_count].dval = val;
    g_param_count++;
    return 1;
}

int32_t aria_mysql_param_add_null(void) {
    if (g_param_count >= MAX_PARAMS) return 0;
    g_params[g_param_count].type = P_NULL;
    g_param_count++;
    return 1;
}

/* ── parameterized query ──────────────────────────────────────── */

int32_t aria_mysql_query(int32_t conn_id, const char *sql) {
    if (conn_id < 0 || conn_id >= MAX_CONNS || !g_conns[conn_id]) return 0;
    clear_result(conn_id);

    MYSQL_STMT *stmt = mysql_stmt_init(g_conns[conn_id]);
    if (!stmt) {
        fprintf(stderr, "[aria-mysql] stmt_init failed\n");
        return 0;
    }

    if (mysql_stmt_prepare(stmt, sql, strlen(sql)) != 0) {
        fprintf(stderr, "[aria-mysql] prepare error: %s\n",
                mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        g_param_count = 0;
        return 0;
    }

    /* Bind params */
    MYSQL_BIND binds[MAX_PARAMS];
    unsigned long lengths[MAX_PARAMS];
    bool nulls[MAX_PARAMS];
    memset(binds, 0, sizeof(binds));
    memset(nulls, 0, sizeof(nulls));

    for (int i = 0; i < g_param_count; i++) {
        switch (g_params[i].type) {
        case P_TEXT:
            binds[i].buffer_type = MYSQL_TYPE_STRING;
            binds[i].buffer = g_params[i].text;
            lengths[i] = strlen(g_params[i].text);
            binds[i].length = &lengths[i];
            binds[i].is_null = &nulls[i];
            break;
        case P_INT:
            binds[i].buffer_type = MYSQL_TYPE_LONGLONG;
            binds[i].buffer = &g_params[i].ival;
            binds[i].is_null = &nulls[i];
            break;
        case P_DOUBLE:
            binds[i].buffer_type = MYSQL_TYPE_DOUBLE;
            binds[i].buffer = &g_params[i].dval;
            binds[i].is_null = &nulls[i];
            break;
        case P_NULL:
            binds[i].buffer_type = MYSQL_TYPE_NULL;
            nulls[i] = 1;
            binds[i].is_null = &nulls[i];
            break;
        }
    }

    if (g_param_count > 0) {
        if (mysql_stmt_bind_param(stmt, binds) != 0) {
            fprintf(stderr, "[aria-mysql] bind error: %s\n",
                    mysql_stmt_error(stmt));
            mysql_stmt_close(stmt);
            g_param_count = 0;
            return 0;
        }
    }

    if (mysql_stmt_execute(stmt) != 0) {
        fprintf(stderr, "[aria-mysql] execute error: %s\n",
                mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        g_param_count = 0;
        return 0;
    }

    g_param_count = 0;

    /* For SELECT: get metadata and store result */
    MYSQL_RES *meta = mysql_stmt_result_metadata(stmt);
    if (meta) {
        /* SELECT query — store result, then fetch via text protocol
         * by re-executing as regular query. Stmt API returns binary which
         * is complex. Instead, just get the column count and close stmt.
         */
        unsigned int nfields = mysql_num_fields(meta);
        mysql_free_result(meta);
        mysql_stmt_close(stmt);

        /* Re-execute as regular query — safe because the params were
         * already bound/escaped via the stmt API above */
        /* Actually, we need to re-query with escaped values.
         * Let's use a different approach: store the stmt result directly. */

        /* Better approach: use mysql_stmt + result binding */
        /* Let's re-prepare and bind result */
        stmt = mysql_stmt_init(g_conns[conn_id]);
        mysql_stmt_prepare(stmt, sql, strlen(sql));

        /* Re-bind params (reuse g_params — they're still valid) */
        /* Actually we cleared g_param_count. For simplicity, use text protocol
         * with escaped strings. */
        (void)nfields;
        mysql_stmt_close(stmt);

        /* Fall back to simple query with escaped values */
        /* Build escaped query string manually */
        /* This gets complex. Let's use a simpler approach:
         * For SELECT with params, build the escaped query directly. */
        return 0; /* TODO: implement properly */
    }

    /* Non-SELECT: just get affected rows */
    mysql_stmt_close(stmt);
    return 1;
}

/* Simpler approach: execute parameterized query by building escaped SQL */
int32_t aria_mysql_execute(int32_t conn_id, const char *sql) {
    if (conn_id < 0 || conn_id >= MAX_CONNS || !g_conns[conn_id]) return 0;
    clear_result(conn_id);

    MYSQL_STMT *stmt = mysql_stmt_init(g_conns[conn_id]);
    if (!stmt) return 0;

    if (mysql_stmt_prepare(stmt, sql, strlen(sql)) != 0) {
        fprintf(stderr, "[aria-mysql] prepare error: %s\n",
                mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        g_param_count = 0;
        return 0;
    }

    MYSQL_BIND binds[MAX_PARAMS];
    unsigned long lengths[MAX_PARAMS];
    bool nulls[MAX_PARAMS];
    memset(binds, 0, sizeof(binds));
    memset(nulls, 0, sizeof(nulls));

    for (int i = 0; i < g_param_count; i++) {
        switch (g_params[i].type) {
        case P_TEXT:
            binds[i].buffer_type = MYSQL_TYPE_STRING;
            binds[i].buffer = g_params[i].text;
            lengths[i] = strlen(g_params[i].text);
            binds[i].length = &lengths[i];
            binds[i].is_null = &nulls[i];
            break;
        case P_INT:
            binds[i].buffer_type = MYSQL_TYPE_LONGLONG;
            binds[i].buffer = &g_params[i].ival;
            binds[i].is_null = &nulls[i];
            break;
        case P_DOUBLE:
            binds[i].buffer_type = MYSQL_TYPE_DOUBLE;
            binds[i].buffer = &g_params[i].dval;
            binds[i].is_null = &nulls[i];
            break;
        case P_NULL:
            binds[i].buffer_type = MYSQL_TYPE_NULL;
            nulls[i] = 1;
            binds[i].is_null = &nulls[i];
            break;
        }
    }

    if (g_param_count > 0) {
        if (mysql_stmt_bind_param(stmt, binds) != 0) {
            fprintf(stderr, "[aria-mysql] bind error: %s\n",
                    mysql_stmt_error(stmt));
            mysql_stmt_close(stmt);
            g_param_count = 0;
            return 0;
        }
    }

    if (mysql_stmt_execute(stmt) != 0) {
        fprintf(stderr, "[aria-mysql] execute error: %s\n",
                mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        g_param_count = 0;
        return 0;
    }

    g_last_affected[conn_id] = (int64_t)mysql_stmt_affected_rows(stmt);
    g_param_count = 0;
    mysql_stmt_close(stmt);
    return 1;
}

/* ── SELECT with params — uses stmt bind result ───────────────── */

/* Result column buffers for stmt-based SELECT */
static char     g_col_bufs[MAX_CONNS][MAX_COLS][COL_BUF_SZ];
static long     g_col_lengths[MAX_CONNS][MAX_COLS];
static bool  g_col_nulls[MAX_CONNS][MAX_COLS];
static bool  g_col_errors[MAX_CONNS][MAX_COLS];
static int      g_nrows[MAX_CONNS];
static int      g_row_cursor[MAX_CONNS];
static MYSQL_STMT *g_stmts[MAX_CONNS]; /* active stmt for SELECT */

int32_t aria_mysql_select(int32_t conn_id, const char *sql) {
    if (conn_id < 0 || conn_id >= MAX_CONNS || !g_conns[conn_id]) return 0;
    clear_result(conn_id);

    /* Close any previous stmt */
    if (g_stmts[conn_id]) {
        mysql_stmt_close(g_stmts[conn_id]);
        g_stmts[conn_id] = NULL;
    }

    MYSQL_STMT *stmt = mysql_stmt_init(g_conns[conn_id]);
    if (!stmt) return 0;

    if (mysql_stmt_prepare(stmt, sql, strlen(sql)) != 0) {
        fprintf(stderr, "[aria-mysql] select prepare error: %s\n",
                mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        g_param_count = 0;
        return 0;
    }

    /* Bind input params */
    MYSQL_BIND binds[MAX_PARAMS];
    unsigned long lengths[MAX_PARAMS];
    bool nulls[MAX_PARAMS];
    memset(binds, 0, sizeof(binds));
    memset(nulls, 0, sizeof(nulls));

    for (int i = 0; i < g_param_count; i++) {
        switch (g_params[i].type) {
        case P_TEXT:
            binds[i].buffer_type = MYSQL_TYPE_STRING;
            binds[i].buffer = g_params[i].text;
            lengths[i] = strlen(g_params[i].text);
            binds[i].length = &lengths[i];
            binds[i].is_null = &nulls[i];
            break;
        case P_INT:
            binds[i].buffer_type = MYSQL_TYPE_LONGLONG;
            binds[i].buffer = &g_params[i].ival;
            binds[i].is_null = &nulls[i];
            break;
        case P_DOUBLE:
            binds[i].buffer_type = MYSQL_TYPE_DOUBLE;
            binds[i].buffer = &g_params[i].dval;
            binds[i].is_null = &nulls[i];
            break;
        case P_NULL:
            binds[i].buffer_type = MYSQL_TYPE_NULL;
            nulls[i] = 1;
            binds[i].is_null = &nulls[i];
            break;
        }
    }

    if (g_param_count > 0) {
        if (mysql_stmt_bind_param(stmt, binds) != 0) {
            fprintf(stderr, "[aria-mysql] select bind error: %s\n",
                    mysql_stmt_error(stmt));
            mysql_stmt_close(stmt);
            g_param_count = 0;
            return 0;
        }
    }
    g_param_count = 0;

    if (mysql_stmt_execute(stmt) != 0) {
        fprintf(stderr, "[aria-mysql] select execute error: %s\n",
                mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return 0;
    }

    /* Store result */
    if (mysql_stmt_store_result(stmt) != 0) {
        fprintf(stderr, "[aria-mysql] store_result error: %s\n",
                mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return 0;
    }

    /* Bind output columns as strings */
    MYSQL_RES *meta = mysql_stmt_result_metadata(stmt);
    if (!meta) {
        mysql_stmt_close(stmt);
        return 0;
    }
    unsigned int nfields = mysql_num_fields(meta);
    mysql_free_result(meta);

    if (nfields > MAX_COLS) nfields = MAX_COLS;
    g_num_fields[conn_id] = nfields;

    MYSQL_BIND res_binds[MAX_COLS];
    memset(res_binds, 0, sizeof(res_binds));

    for (unsigned int i = 0; i < nfields; i++) {
        res_binds[i].buffer_type = MYSQL_TYPE_STRING;
        res_binds[i].buffer = g_col_bufs[conn_id][i];
        res_binds[i].buffer_length = COL_BUF_SZ;
        res_binds[i].length = (unsigned long *)&g_col_lengths[conn_id][i];
        res_binds[i].is_null = &g_col_nulls[conn_id][i];
        res_binds[i].error = &g_col_errors[conn_id][i];
    }

    if (mysql_stmt_bind_result(stmt, res_binds) != 0) {
        fprintf(stderr, "[aria-mysql] bind_result error: %s\n",
                mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return 0;
    }

    g_nrows[conn_id] = (int)mysql_stmt_num_rows(stmt);
    g_row_cursor[conn_id] = -1;
    g_stmts[conn_id] = stmt;

    return 1;
}

/* ── result navigation ────────────────────────────────────────── */

int32_t aria_mysql_next_row(int32_t conn_id) {
    if (conn_id < 0 || conn_id >= MAX_CONNS) return 0;

    /* stmt-based result */
    if (g_stmts[conn_id]) {
        int rc = mysql_stmt_fetch(g_stmts[conn_id]);
        if (rc == 0) {
            g_row_cursor[conn_id]++;
            return 1;
        }
        return 0;
    }

    /* text-protocol result */
    if (!g_results[conn_id]) return 0;
    g_current_row[conn_id] = mysql_fetch_row(g_results[conn_id]);
    if (g_current_row[conn_id]) {
        g_row_cursor[conn_id]++;
        return 1;
    }
    return 0;
}

int32_t aria_mysql_row_count(int32_t conn_id) {
    if (conn_id < 0 || conn_id >= MAX_CONNS) return 0;
    if (g_stmts[conn_id]) return g_nrows[conn_id];
    if (g_results[conn_id]) return (int32_t)mysql_num_rows(g_results[conn_id]);
    return 0;
}

int32_t aria_mysql_column_count(int32_t conn_id) {
    if (conn_id < 0 || conn_id >= MAX_CONNS) return 0;
    return (int32_t)g_num_fields[conn_id];
}

/* Column text — for stmt results, read from bound buffer */
const char *aria_mysql_column_text(int32_t conn_id, int32_t col) {
    if (conn_id < 0 || conn_id >= MAX_CONNS) return "";
    if (col < 0 || col >= (int32_t)g_num_fields[conn_id]) return "";

    if (g_stmts[conn_id]) {
        if (g_col_nulls[conn_id][col]) return "";
        g_col_bufs[conn_id][col][g_col_lengths[conn_id][col]] = '\0';
        return g_col_bufs[conn_id][col];
    }

    if (g_current_row[conn_id] && g_current_row[conn_id][col]) {
        return g_current_row[conn_id][col];
    }
    return "";
}

int64_t aria_mysql_column_int(int32_t conn_id, int32_t col) {
    const char *val = aria_mysql_column_text(conn_id, col);
    if (!val || !*val) return 0;
    return strtol(val, NULL, 10);
}

double aria_mysql_column_double(int32_t conn_id, int32_t col) {
    const char *val = aria_mysql_column_text(conn_id, col);
    if (!val || !*val) return 0.0;
    return strtod(val, NULL);
}

int32_t aria_mysql_column_is_null(int32_t conn_id, int32_t col) {
    if (conn_id < 0 || conn_id >= MAX_CONNS) return 1;
    if (col < 0 || col >= (int32_t)g_num_fields[conn_id]) return 1;

    if (g_stmts[conn_id]) {
        return g_col_nulls[conn_id][col] ? 1 : 0;
    }

    if (g_current_row[conn_id]) {
        return g_current_row[conn_id][col] == NULL ? 1 : 0;
    }
    return 1;
}

int32_t aria_mysql_clear_result(int32_t conn_id) {
    if (conn_id < 0 || conn_id >= MAX_CONNS) return 0;
    if (g_stmts[conn_id]) {
        mysql_stmt_close(g_stmts[conn_id]);
        g_stmts[conn_id] = NULL;
    }
    clear_result(conn_id);
    g_nrows[conn_id] = 0;
    g_row_cursor[conn_id] = 0;
    return 1;
}

/* ── affected rows / last insert id ──────────────────────────── */

int64_t aria_mysql_affected_rows(int32_t conn_id) {
    if (conn_id < 0 || conn_id >= MAX_CONNS || !g_conns[conn_id]) return 0;
    if (g_last_affected[conn_id] >= 0) { int64_t r = g_last_affected[conn_id]; g_last_affected[conn_id] = -1; return r; }
    return (int64_t)mysql_affected_rows(g_conns[conn_id]);
}

int64_t aria_mysql_last_insert_id(int32_t conn_id) {
    if (conn_id < 0 || conn_id >= MAX_CONNS || !g_conns[conn_id]) return 0;
    return (int64_t)mysql_insert_id(g_conns[conn_id]);
}

/* ── transactions ─────────────────────────────────────────────── */

int32_t aria_mysql_begin(int32_t conn_id) {
    return aria_mysql_exec(conn_id, "START TRANSACTION");
}

int32_t aria_mysql_commit(int32_t conn_id) {
    return aria_mysql_exec(conn_id, "COMMIT");
}

int32_t aria_mysql_rollback(int32_t conn_id) {
    return aria_mysql_exec(conn_id, "ROLLBACK");
}

/* ── utility ──────────────────────────────────────────────────── */

const char *aria_mysql_error(int32_t conn_id) {
    if (conn_id < 0 || conn_id >= MAX_CONNS || !g_conns[conn_id]) return "invalid connection";
    return mysql_error(g_conns[conn_id]);
}

int32_t aria_mysql_status(int32_t conn_id) {
    if (conn_id < 0 || conn_id >= MAX_CONNS || !g_conns[conn_id]) return 0;
    return (mysql_ping(g_conns[conn_id]) == 0) ? 1 : 0;
}

/* ── simple query with result set (no params) ─────────────────── */

int32_t aria_mysql_simple_query(int32_t conn_id, const char *sql) {
    if (conn_id < 0 || conn_id >= MAX_CONNS || !g_conns[conn_id]) return 0;
    clear_result(conn_id);
    if (g_stmts[conn_id]) {
        mysql_stmt_close(g_stmts[conn_id]);
        g_stmts[conn_id] = NULL;
    }

    if (mysql_query(g_conns[conn_id], sql) != 0) {
        fprintf(stderr, "[aria-mysql] query error: %s\n",
                mysql_error(g_conns[conn_id]));
        return 0;
    }

    g_results[conn_id] = mysql_store_result(g_conns[conn_id]);
    if (!g_results[conn_id]) {
        /* Non-SELECT or error */
        if (mysql_field_count(g_conns[conn_id]) == 0) return 1; /* was non-SELECT */
        fprintf(stderr, "[aria-mysql] store_result error: %s\n",
                mysql_error(g_conns[conn_id]));
        return 0;
    }

    g_num_fields[conn_id] = mysql_num_fields(g_results[conn_id]);
    g_row_cursor[conn_id] = -1;
    return 1;
}

/* ── test assertion helpers (C-side, avoids string return bug) ── */

static int g_test_pass = 0;
static int g_test_fail = 0;

void aria_mysql_assert_int_eq(int64_t actual, int64_t expected, const char *msg) {
    if (actual == expected) {
        printf("  PASS: %s\n", msg);
        g_test_pass++;
    } else {
        printf("  FAIL: %s — expected %ld, got %ld\n", msg, (long)expected, (long)actual);
        g_test_fail++;
    }
}

void aria_mysql_assert_true(int8_t val, const char *msg) {
    if (val) {
        printf("  PASS: %s\n", msg);
        g_test_pass++;
    } else {
        printf("  FAIL: %s — expected true\n", msg);
        g_test_fail++;
    }
}

void aria_mysql_assert_col_text_eq(int32_t conn_id, int32_t col,
                                    const char *expected, const char *msg) {
    const char *actual = aria_mysql_column_text(conn_id, col);
    if (strcmp(actual, expected) == 0) {
        printf("  PASS: %s (value=\"%s\")\n", msg, actual);
        g_test_pass++;
    } else {
        printf("  FAIL: %s — expected \"%s\", got \"%s\"\n", msg, expected, actual);
        g_test_fail++;
    }
}

void aria_mysql_assert_col_int_eq(int32_t conn_id, int32_t col,
                                   int64_t expected, const char *msg) {
    int64_t actual = aria_mysql_column_int(conn_id, col);
    if (actual == expected) {
        printf("  PASS: %s (value=%ld)\n", msg, (long)actual);
        g_test_pass++;
    } else {
        printf("  FAIL: %s — expected %ld, got %ld\n", msg, (long)expected, (long)actual);
        g_test_fail++;
    }
}

void aria_mysql_assert_col_is_null(int32_t conn_id, int32_t col, const char *msg) {
    if (aria_mysql_column_is_null(conn_id, col)) {
        printf("  PASS: %s\n", msg);
        g_test_pass++;
    } else {
        printf("  FAIL: %s — expected NULL\n", msg);
        g_test_fail++;
    }
}

void aria_mysql_assert_version_ok(int32_t conn_id, const char *msg) {
    if (conn_id < 0 || conn_id >= MAX_CONNS || !g_conns[conn_id]) {
        printf("  FAIL: %s — no connection\n", msg);
        g_test_fail++;
        return;
    }
    const char *ver = mysql_get_server_info(g_conns[conn_id]);
    if (ver && *ver) {
        printf("  PASS: %s (version=%s)\n", msg, ver);
        g_test_pass++;
    } else {
        printf("  FAIL: %s — could not get version\n", msg);
        g_test_fail++;
    }
}

void aria_mysql_assert_affected_eq(int32_t conn_id, int64_t expected, const char *msg) {
    int64_t actual = aria_mysql_affected_rows(conn_id);
    if (actual == expected) {
        printf("  PASS: %s (affected=%ld)\n", msg, (long)actual);
        g_test_pass++;
    } else {
        printf("  FAIL: %s — expected %ld affected, got %ld\n", msg, (long)expected, (long)actual);
        g_test_fail++;
    }
}

void aria_mysql_test_summary(void) {
    int total = g_test_pass + g_test_fail;
    printf("\n========================================\n");
    printf("passed=%d failed=%d total=%d\n", g_test_pass, g_test_fail, total);
    if (g_test_fail == 0)
        printf("RESULT: ALL TESTS PASSED\n");
    else
        printf("RESULT: SOME TESTS FAILED\n");
    printf("========================================\n");
}
