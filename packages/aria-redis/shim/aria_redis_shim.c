/*
 * aria_redis_shim.c — C shim for Aria <-> hiredis (Redis)
 *
 * Design mirrors other DB shims:
 *   - Connection pool (16 slots)
 *   - Handle helpers: last_conn() for move-semantics workaround
 *   - "disconnect" instead of "close" (compiler bug workaround)
 *   - C-side string assertion helpers (string return corruption workaround)
 *   - Argument staging for variadic commands
 *
 * Build:
 *   cc -O2 -shared -fPIC -Wall -o libaria_redis_shim.so \
 *      aria_redis_shim.c -lhiredis
 */

#include <hiredis/hiredis.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* ── pools ────────────────────────────────────────────────────── */

#define MAX_CONNS    16
#define MAX_ARGS     32
#define REPLY_BUF_SZ 4096

static redisContext *g_conns[MAX_CONNS];
static int32_t       g_last_conn = -1;

/* last reply caching per-connection */
static redisReply *g_reply[MAX_CONNS];

/* staged arguments for multi-arg commands (MSET, LPUSH list, etc.) */

/* ── connection management ────────────────────────────────────── */

int32_t aria_redis_connect(const char *host, int32_t port) {
    for (int i = 0; i < MAX_CONNS; i++) {
        if (!g_conns[i]) {
            g_conns[i] = redisConnect(host, port);
            if (!g_conns[i] || g_conns[i]->err) {
                if (g_conns[i]) {
                    fprintf(stderr, "[aria-redis] connect error: %s\n",
                            g_conns[i]->errstr);
                    redisFree(g_conns[i]);
                    g_conns[i] = NULL;
                }
                return -1;
            }
            g_last_conn = i;
            g_reply[i] = NULL;
            return i;
        }
    }
    return -1; /* pool full */
}

int32_t aria_redis_connect_auth(const char *host, int32_t port,
                                const char *password) {
    int32_t id = aria_redis_connect(host, port);
    if (id < 0) return id;

    redisReply *r = redisCommand(g_conns[id], "AUTH %s", password);
    if (!r || r->type == REDIS_REPLY_ERROR) {
        if (r) {
            fprintf(stderr, "[aria-redis] AUTH error: %s\n", r->str);
            freeReplyObject(r);
        }
        redisFree(g_conns[id]);
        g_conns[id] = NULL;
        return -1;
    }
    freeReplyObject(r);
    return id;
}

int32_t aria_redis_disconnect(int32_t conn_id) {
    if (conn_id < 0 || conn_id >= MAX_CONNS || !g_conns[conn_id]) return 0;
    if (g_reply[conn_id]) { freeReplyObject(g_reply[conn_id]); g_reply[conn_id] = NULL; }
    redisFree(g_conns[conn_id]);
    g_conns[conn_id] = NULL;
    return 1;
}

int32_t aria_redis_last_conn(void) { return g_last_conn; }

/* ── reply management ─────────────────────────────────────────── */

static void clear_reply(int32_t conn_id) {
    if (g_reply[conn_id]) {
        freeReplyObject(g_reply[conn_id]);
        g_reply[conn_id] = NULL;
    }
}

/* ── core commands ─── GET, SET, DEL, EXISTS, EXPIRE, TTL ──── */

int32_t aria_redis_set(int32_t conn_id, const char *key, const char *value) {
    if (conn_id < 0 || conn_id >= MAX_CONNS || !g_conns[conn_id]) return 0;
    clear_reply(conn_id);
    g_reply[conn_id] = redisCommand(g_conns[conn_id], "SET %s %s", key, value);
    if (!g_reply[conn_id] || g_reply[conn_id]->type == REDIS_REPLY_ERROR) {
        if (g_reply[conn_id])
            fprintf(stderr, "[aria-redis] SET error: %s\n", g_reply[conn_id]->str);
        return 0;
    }
    return 1;
}

int32_t aria_redis_set_ex(int32_t conn_id, const char *key, const char *value,
                          int32_t seconds) {
    if (conn_id < 0 || conn_id >= MAX_CONNS || !g_conns[conn_id]) return 0;
    clear_reply(conn_id);
    g_reply[conn_id] = redisCommand(g_conns[conn_id], "SET %s %s EX %d",
                                    key, value, seconds);
    if (!g_reply[conn_id] || g_reply[conn_id]->type == REDIS_REPLY_ERROR) return 0;
    return 1;
}

int32_t aria_redis_get(int32_t conn_id, const char *key) {
    if (conn_id < 0 || conn_id >= MAX_CONNS || !g_conns[conn_id]) return 0;
    clear_reply(conn_id);
    g_reply[conn_id] = redisCommand(g_conns[conn_id], "GET %s", key);
    if (!g_reply[conn_id]) return 0;
    if (g_reply[conn_id]->type == REDIS_REPLY_NIL) return 2; /* key not found */
    if (g_reply[conn_id]->type == REDIS_REPLY_ERROR) return 0;
    return 1; /* success, value in reply->str */
}

/* Return reply string — used by C assertion helpers, not Aria directly */
const char *aria_redis_reply_str(int32_t conn_id) {
    if (conn_id < 0 || conn_id >= MAX_CONNS || !g_reply[conn_id]) return "";
    if (g_reply[conn_id]->type == REDIS_REPLY_STRING ||
        g_reply[conn_id]->type == REDIS_REPLY_STATUS)
        return g_reply[conn_id]->str;
    return "";
}

int64_t aria_redis_reply_int(int32_t conn_id) {
    if (conn_id < 0 || conn_id >= MAX_CONNS || !g_reply[conn_id]) return 0;
    if (g_reply[conn_id]->type == REDIS_REPLY_INTEGER)
        return (int64_t)g_reply[conn_id]->integer;
    return 0;
}

int32_t aria_redis_del(int32_t conn_id, const char *key) {
    if (conn_id < 0 || conn_id >= MAX_CONNS || !g_conns[conn_id]) return 0;
    clear_reply(conn_id);
    g_reply[conn_id] = redisCommand(g_conns[conn_id], "DEL %s", key);
    if (!g_reply[conn_id] || g_reply[conn_id]->type == REDIS_REPLY_ERROR) return 0;
    return (int32_t)g_reply[conn_id]->integer; /* number of keys deleted */
}

int32_t aria_redis_exists(int32_t conn_id, const char *key) {
    if (conn_id < 0 || conn_id >= MAX_CONNS || !g_conns[conn_id]) return -1;
    clear_reply(conn_id);
    g_reply[conn_id] = redisCommand(g_conns[conn_id], "EXISTS %s", key);
    if (!g_reply[conn_id] || g_reply[conn_id]->type == REDIS_REPLY_ERROR) return -1;
    return (int32_t)g_reply[conn_id]->integer; /* 1 if exists, 0 if not */
}

int32_t aria_redis_expire(int32_t conn_id, const char *key, int32_t seconds) {
    if (conn_id < 0 || conn_id >= MAX_CONNS || !g_conns[conn_id]) return 0;
    clear_reply(conn_id);
    g_reply[conn_id] = redisCommand(g_conns[conn_id], "EXPIRE %s %d",
                                    key, seconds);
    if (!g_reply[conn_id] || g_reply[conn_id]->type == REDIS_REPLY_ERROR) return 0;
    return (int32_t)g_reply[conn_id]->integer; /* 1 if set, 0 if key doesn't exist */
}

int64_t aria_redis_ttl(int32_t conn_id, const char *key) {
    if (conn_id < 0 || conn_id >= MAX_CONNS || !g_conns[conn_id]) return -3;
    clear_reply(conn_id);
    g_reply[conn_id] = redisCommand(g_conns[conn_id], "TTL %s", key);
    if (!g_reply[conn_id] || g_reply[conn_id]->type == REDIS_REPLY_ERROR) return -3;
    return (int64_t)g_reply[conn_id]->integer; /* -2 if not exist, -1 if no expiry */
}

int32_t aria_redis_incr(int32_t conn_id, const char *key) {
    if (conn_id < 0 || conn_id >= MAX_CONNS || !g_conns[conn_id]) return 0;
    clear_reply(conn_id);
    g_reply[conn_id] = redisCommand(g_conns[conn_id], "INCR %s", key);
    if (!g_reply[conn_id] || g_reply[conn_id]->type == REDIS_REPLY_ERROR) return 0;
    return 1;
}

int32_t aria_redis_incrby(int32_t conn_id, const char *key, int64_t amount) {
    if (conn_id < 0 || conn_id >= MAX_CONNS || !g_conns[conn_id]) return 0;
    clear_reply(conn_id);
    g_reply[conn_id] = redisCommand(g_conns[conn_id], "INCRBY %s %lld",
                                    key, (long long)amount);
    if (!g_reply[conn_id] || g_reply[conn_id]->type == REDIS_REPLY_ERROR) return 0;
    return 1;
}

/* ── list commands: LPUSH, RPUSH, LPOP, RPOP, LLEN, LRANGE ── */

int32_t aria_redis_lpush(int32_t conn_id, const char *key, const char *value) {
    if (conn_id < 0 || conn_id >= MAX_CONNS || !g_conns[conn_id]) return 0;
    clear_reply(conn_id);
    g_reply[conn_id] = redisCommand(g_conns[conn_id], "LPUSH %s %s", key, value);
    if (!g_reply[conn_id] || g_reply[conn_id]->type == REDIS_REPLY_ERROR) return 0;
    return (int32_t)g_reply[conn_id]->integer; /* new list length */
}

int32_t aria_redis_rpush(int32_t conn_id, const char *key, const char *value) {
    if (conn_id < 0 || conn_id >= MAX_CONNS || !g_conns[conn_id]) return 0;
    clear_reply(conn_id);
    g_reply[conn_id] = redisCommand(g_conns[conn_id], "RPUSH %s %s", key, value);
    if (!g_reply[conn_id] || g_reply[conn_id]->type == REDIS_REPLY_ERROR) return 0;
    return (int32_t)g_reply[conn_id]->integer;
}

int32_t aria_redis_lpop(int32_t conn_id, const char *key) {
    if (conn_id < 0 || conn_id >= MAX_CONNS || !g_conns[conn_id]) return 0;
    clear_reply(conn_id);
    g_reply[conn_id] = redisCommand(g_conns[conn_id], "LPOP %s", key);
    if (!g_reply[conn_id]) return 0;
    if (g_reply[conn_id]->type == REDIS_REPLY_NIL) return 2; /* empty list */
    if (g_reply[conn_id]->type == REDIS_REPLY_ERROR) return 0;
    return 1; /* value in reply_str */
}

int32_t aria_redis_rpop(int32_t conn_id, const char *key) {
    if (conn_id < 0 || conn_id >= MAX_CONNS || !g_conns[conn_id]) return 0;
    clear_reply(conn_id);
    g_reply[conn_id] = redisCommand(g_conns[conn_id], "RPOP %s", key);
    if (!g_reply[conn_id]) return 0;
    if (g_reply[conn_id]->type == REDIS_REPLY_NIL) return 2;
    if (g_reply[conn_id]->type == REDIS_REPLY_ERROR) return 0;
    return 1;
}

int64_t aria_redis_llen(int32_t conn_id, const char *key) {
    if (conn_id < 0 || conn_id >= MAX_CONNS || !g_conns[conn_id]) return -1;
    clear_reply(conn_id);
    g_reply[conn_id] = redisCommand(g_conns[conn_id], "LLEN %s", key);
    if (!g_reply[conn_id] || g_reply[conn_id]->type == REDIS_REPLY_ERROR) return -1;
    return (int64_t)g_reply[conn_id]->integer;
}

/* LRANGE returns array reply — store elements for iteration */
static char     g_list_buf[MAX_CONNS][MAX_ARGS][REPLY_BUF_SZ];
static int32_t  g_list_count[MAX_CONNS];
static int32_t  g_list_cursor[MAX_CONNS];

int32_t aria_redis_lrange(int32_t conn_id, const char *key,
                          int32_t start, int32_t stop) {
    if (conn_id < 0 || conn_id >= MAX_CONNS || !g_conns[conn_id]) return 0;
    clear_reply(conn_id);
    g_list_count[conn_id] = 0;
    g_list_cursor[conn_id] = 0;
    g_reply[conn_id] = redisCommand(g_conns[conn_id], "LRANGE %s %d %d",
                                    key, start, stop);
    if (!g_reply[conn_id] || g_reply[conn_id]->type != REDIS_REPLY_ARRAY) return 0;

    int n = (int)g_reply[conn_id]->elements;
    if (n > MAX_ARGS) n = MAX_ARGS;
    for (int i = 0; i < n; i++) {
        if (g_reply[conn_id]->element[i]->type == REDIS_REPLY_STRING) {
            strncpy(g_list_buf[conn_id][i],
                    g_reply[conn_id]->element[i]->str, REPLY_BUF_SZ - 1);
            g_list_buf[conn_id][i][REPLY_BUF_SZ - 1] = '\0';
        } else {
            g_list_buf[conn_id][i][0] = '\0';
        }
    }
    g_list_count[conn_id] = n;
    return n;
}

int32_t aria_redis_list_count(int32_t conn_id) {
    if (conn_id < 0 || conn_id >= MAX_CONNS) return 0;
    return g_list_count[conn_id];
}

/* ── hash commands: HSET, HGET, HDEL, HEXISTS, HLEN ────────── */

int32_t aria_redis_hset(int32_t conn_id, const char *key, const char *field,
                        const char *value) {
    if (conn_id < 0 || conn_id >= MAX_CONNS || !g_conns[conn_id]) return -1;
    clear_reply(conn_id);
    g_reply[conn_id] = redisCommand(g_conns[conn_id], "HSET %s %s %s",
                                    key, field, value);
    if (!g_reply[conn_id] || g_reply[conn_id]->type == REDIS_REPLY_ERROR) return -1;
    return (int32_t)g_reply[conn_id]->integer; /* 1 if new field, 0 if updated */
}

int32_t aria_redis_hget(int32_t conn_id, const char *key, const char *field) {
    if (conn_id < 0 || conn_id >= MAX_CONNS || !g_conns[conn_id]) return 0;
    clear_reply(conn_id);
    g_reply[conn_id] = redisCommand(g_conns[conn_id], "HGET %s %s", key, field);
    if (!g_reply[conn_id]) return 0;
    if (g_reply[conn_id]->type == REDIS_REPLY_NIL) return 2;
    if (g_reply[conn_id]->type == REDIS_REPLY_ERROR) return 0;
    return 1;
}

int32_t aria_redis_hdel(int32_t conn_id, const char *key, const char *field) {
    if (conn_id < 0 || conn_id >= MAX_CONNS || !g_conns[conn_id]) return 0;
    clear_reply(conn_id);
    g_reply[conn_id] = redisCommand(g_conns[conn_id], "HDEL %s %s", key, field);
    if (!g_reply[conn_id] || g_reply[conn_id]->type == REDIS_REPLY_ERROR) return 0;
    return (int32_t)g_reply[conn_id]->integer;
}

int32_t aria_redis_hexists(int32_t conn_id, const char *key, const char *field) {
    if (conn_id < 0 || conn_id >= MAX_CONNS || !g_conns[conn_id]) return -1;
    clear_reply(conn_id);
    g_reply[conn_id] = redisCommand(g_conns[conn_id], "HEXISTS %s %s",
                                    key, field);
    if (!g_reply[conn_id] || g_reply[conn_id]->type == REDIS_REPLY_ERROR) return -1;
    return (int32_t)g_reply[conn_id]->integer;
}

int64_t aria_redis_hlen(int32_t conn_id, const char *key) {
    if (conn_id < 0 || conn_id >= MAX_CONNS || !g_conns[conn_id]) return -1;
    clear_reply(conn_id);
    g_reply[conn_id] = redisCommand(g_conns[conn_id], "HLEN %s", key);
    if (!g_reply[conn_id] || g_reply[conn_id]->type == REDIS_REPLY_ERROR) return -1;
    return (int64_t)g_reply[conn_id]->integer;
}

/* ── set commands: SADD, SREM, SISMEMBER, SCARD ───────────── */

int32_t aria_redis_sadd(int32_t conn_id, const char *key, const char *member) {
    if (conn_id < 0 || conn_id >= MAX_CONNS || !g_conns[conn_id]) return -1;
    clear_reply(conn_id);
    g_reply[conn_id] = redisCommand(g_conns[conn_id], "SADD %s %s", key, member);
    if (!g_reply[conn_id] || g_reply[conn_id]->type == REDIS_REPLY_ERROR) return -1;
    return (int32_t)g_reply[conn_id]->integer;
}

int32_t aria_redis_srem(int32_t conn_id, const char *key, const char *member) {
    if (conn_id < 0 || conn_id >= MAX_CONNS || !g_conns[conn_id]) return -1;
    clear_reply(conn_id);
    g_reply[conn_id] = redisCommand(g_conns[conn_id], "SREM %s %s", key, member);
    if (!g_reply[conn_id] || g_reply[conn_id]->type == REDIS_REPLY_ERROR) return -1;
    return (int32_t)g_reply[conn_id]->integer;
}

int32_t aria_redis_sismember(int32_t conn_id, const char *key,
                             const char *member) {
    if (conn_id < 0 || conn_id >= MAX_CONNS || !g_conns[conn_id]) return -1;
    clear_reply(conn_id);
    g_reply[conn_id] = redisCommand(g_conns[conn_id], "SISMEMBER %s %s",
                                    key, member);
    if (!g_reply[conn_id] || g_reply[conn_id]->type == REDIS_REPLY_ERROR) return -1;
    return (int32_t)g_reply[conn_id]->integer;
}

int64_t aria_redis_scard(int32_t conn_id, const char *key) {
    if (conn_id < 0 || conn_id >= MAX_CONNS || !g_conns[conn_id]) return -1;
    clear_reply(conn_id);
    g_reply[conn_id] = redisCommand(g_conns[conn_id], "SCARD %s", key);
    if (!g_reply[conn_id] || g_reply[conn_id]->type == REDIS_REPLY_ERROR) return -1;
    return (int64_t)g_reply[conn_id]->integer;
}

/* ── generic command (for anything not wrapped) ───────────── */

int32_t aria_redis_command(int32_t conn_id, const char *cmd) {
    if (conn_id < 0 || conn_id >= MAX_CONNS || !g_conns[conn_id]) return 0;
    clear_reply(conn_id);
    g_reply[conn_id] = redisCommand(g_conns[conn_id], cmd);
    if (!g_reply[conn_id]) return 0;
    if (g_reply[conn_id]->type == REDIS_REPLY_ERROR) {
        fprintf(stderr, "[aria-redis] command error: %s\n", g_reply[conn_id]->str);
        return 0;
    }
    return 1;
}

/* ── PING ─────────────────────────────────────────────────────── */

int32_t aria_redis_ping(int32_t conn_id) {
    if (conn_id < 0 || conn_id >= MAX_CONNS || !g_conns[conn_id]) return 0;
    clear_reply(conn_id);
    g_reply[conn_id] = redisCommand(g_conns[conn_id], "PING");
    if (!g_reply[conn_id]) return 0;
    if (g_reply[conn_id]->type == REDIS_REPLY_STATUS &&
        strcmp(g_reply[conn_id]->str, "PONG") == 0)
        return 1;
    return 0;
}

/* ── DBSIZE ───────────────────────────────────────────────────── */

int64_t aria_redis_dbsize(int32_t conn_id) {
    if (conn_id < 0 || conn_id >= MAX_CONNS || !g_conns[conn_id]) return -1;
    clear_reply(conn_id);
    g_reply[conn_id] = redisCommand(g_conns[conn_id], "DBSIZE");
    if (!g_reply[conn_id] || g_reply[conn_id]->type != REDIS_REPLY_INTEGER) return -1;
    return (int64_t)g_reply[conn_id]->integer;
}

/* ── FLUSHDB (use with caution — test cleanup) ─────────────── */

int32_t aria_redis_flushdb(int32_t conn_id) {
    if (conn_id < 0 || conn_id >= MAX_CONNS || !g_conns[conn_id]) return 0;
    clear_reply(conn_id);
    g_reply[conn_id] = redisCommand(g_conns[conn_id], "FLUSHDB");
    if (!g_reply[conn_id] || g_reply[conn_id]->type == REDIS_REPLY_ERROR) return 0;
    return 1;
}

/* ── SELECT db ─────────────────────────────────────────────────── */

int32_t aria_redis_select_db(int32_t conn_id, int32_t db) {
    if (conn_id < 0 || conn_id >= MAX_CONNS || !g_conns[conn_id]) return 0;
    clear_reply(conn_id);
    g_reply[conn_id] = redisCommand(g_conns[conn_id], "SELECT %d", db);
    if (!g_reply[conn_id] || g_reply[conn_id]->type == REDIS_REPLY_ERROR) return 0;
    return 1;
}

/* ═══════════════════════════════════════════════════════════════
 *  C-side Test assertion helpers
 * ═══════════════════════════════════════════════════════════════ */

static int g_pass = 0, g_fail = 0;

void aria_redis_assert_true(int8_t val, const char *msg) {
    if (val) { printf("  PASS: %s\n", msg); g_pass++; }
    else     { printf("  FAIL: %s\n", msg); g_fail++; }
}

void aria_redis_assert_int_eq(int64_t actual, int64_t expected, const char *msg) {
    if (actual == expected) {
        printf("  PASS: %s (value=%lld)\n", msg, (long long)actual);
        g_pass++;
    } else {
        printf("  FAIL: %s — expected %lld, got %lld\n", msg,
               (long long)expected, (long long)actual);
        g_fail++;
    }
}

void aria_redis_assert_reply_eq(int32_t conn_id, const char *expected,
                                const char *msg) {
    const char *actual = aria_redis_reply_str(conn_id);
    if (strcmp(actual, expected) == 0) {
        printf("  PASS: %s (value=\"%s\")\n", msg, actual);
        g_pass++;
    } else {
        printf("  FAIL: %s — expected \"%s\", got \"%s\"\n",
               msg, expected, actual);
        g_fail++;
    }
}

void aria_redis_assert_list_element_eq(int32_t conn_id, int32_t idx,
                                       const char *expected, const char *msg) {
    if (conn_id < 0 || conn_id >= MAX_CONNS ||
        idx < 0 || idx >= g_list_count[conn_id]) {
        printf("  FAIL: %s — index %d out of range\n", msg, idx);
        g_fail++;
        return;
    }
    const char *actual = g_list_buf[conn_id][idx];
    if (strcmp(actual, expected) == 0) {
        printf("  PASS: %s (value=\"%s\")\n", msg, actual);
        g_pass++;
    } else {
        printf("  FAIL: %s — expected \"%s\", got \"%s\"\n",
               msg, expected, actual);
        g_fail++;
    }
}

void aria_redis_test_summary(void) {
    printf("\n========================================\n");
    printf("passed=%d failed=%d total=%d\n", g_pass, g_fail, g_pass + g_fail);
    printf("RESULT: %s\n", g_fail == 0 ? "ALL TESTS PASSED" : "SOME TESTS FAILED");
    printf("========================================\n");
}
