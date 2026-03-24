# Aria Database Client Libraries — Guide

This guide covers the four database client libraries available in the Aria package ecosystem.

## Overview

| Package | Database | API Prefix | System Library | Server Required |
|---------|----------|-----------|----------------|-----------------|
| aria-sqlite | SQLite 3 | `sqlite_*` | libsqlite3 | No (embedded) |
| aria-postgres | PostgreSQL | `pg_*` | libpq | Yes |
| aria-mysql | MySQL / MariaDB | `mysql_db_*` | libmysqlclient | Yes |
| aria-redis | Redis | `redis_*` | libhiredis | Yes |

All SQL libraries use **parameterized queries** to prevent SQL injection attacks.

## Prerequisites

```bash
# Install the dev libraries you need:
sudo apt install libsqlite3-dev    # SQLite
sudo apt install libpq-dev         # PostgreSQL
sudo apt install libmysqlclient-dev # MySQL (or libmariadb-dev)
sudo apt install libhiredis-dev    # Redis
```

## Building

Each package has a C shim that bridges Aria's FFI to the native C library:

```bash
cd aria-packages/packages/aria-sqlite/shim
cc -O2 -shared -fPIC -Wall -o libaria_sqlite_shim.so aria_sqlite_shim.c -lsqlite3
```

Build flags per database:

| Package | Include Path | Link Flag |
|---------|-------------|-----------|
| aria-sqlite | (default) | `-lsqlite3` |
| aria-postgres | `-I/usr/include/postgresql` | `-lpq` |
| aria-mysql | `-I/usr/include/mysql` | `-lmysqlclient` |
| aria-redis | (default) | `-lhiredis` |

Compile your Aria program:

```bash
ariac my_program.aria -L shim -laria_sqlite_shim -lsqlite3 -o my_program
LD_LIBRARY_PATH=shim ./my_program
```

## Common Patterns

### Connection Management

All drivers use a connection pool (16 slots). `connect()` returns a connection ID (int32). Due to Aria's move semantics, use `last_conn()` / `last_db()` to get a fresh handle before each call:

```
int32:db = sqlite_open(":memory:");
int32:d = sqlite_last_db();   // fresh handle
sqlite_exec(d, "CREATE TABLE t(id INTEGER)");
int32:d2 = sqlite_last_db();  // fresh handle again
sqlite_disconnect(d2);
```

### Parameterized Queries (SQL Injection Safety)

Stage parameters before executing:

```
// SQLite uses ?1, ?2 placeholders
sqlite_param_add_text("Alice");
sqlite_param_add_int(30i64);
int32:d = sqlite_last_db();
sqlite_execute(d, "INSERT INTO users(name, age) VALUES(?1, ?2)");

// PostgreSQL uses $1, $2 placeholders
pg_param_add_text("Alice");
pg_param_add_int(30i64);
int32:c = pg_conn();
pg_execute(c, "INSERT INTO users(name, age) VALUES($1, $2)");

// MySQL uses ? placeholders
mysql_db_param_add_text("Alice");
mysql_db_param_add_int(30i64);
int32:c = mysql_db_conn();
mysql_db_execute(c, "INSERT INTO users(name, age) VALUES(?, ?)");
```

### Result Iteration

```
int32:d = sqlite_last_db();
sqlite_query(d, "SELECT name, age FROM users");

int32:d2 = sqlite_last_db();
int32:has_row = sqlite_next_row(d2);  // 1 if row available
// Use column accessors: column_int, column_text (C-side helpers)
```

### Transactions

```
int32:c = pg_conn();
pg_begin(c);
// ... operations ...
int32:c2 = pg_conn();
pg_commit(c2);    // or pg_rollback(c2)
```

## SQLite API Reference

| Function | Returns | Description |
|----------|---------|-------------|
| `sqlite_open(path)` | int32 | Open database (`:memory:` for in-memory) |
| `sqlite_last_db()` | int32 | Get last connection handle |
| `sqlite_exec(db, sql)` | int32 | Execute SQL (no results) |
| `sqlite_param_add_text(val)` | int32 | Stage text parameter |
| `sqlite_param_add_int(val)` | int32 | Stage int64 parameter |
| `sqlite_param_add_double(val)` | int32 | Stage float64 parameter |
| `sqlite_param_add_null()` | int32 | Stage NULL parameter |
| `sqlite_query(db, sql)` | int32 | Execute SELECT with staged params |
| `sqlite_execute(db, sql)` | int32 | Execute INSERT/UPDATE/DELETE with params |
| `sqlite_next_row(db)` | int32 | Advance to next row (1=has row, 0=done) |
| `sqlite_column_int(db, col)` | int64 | Get column as integer |
| `sqlite_disconnect(db)` | int32 | Close database |

## PostgreSQL API Reference

| Function | Returns | Description |
|----------|---------|-------------|
| `pg_connect(conninfo)` | int32 | Connect (e.g., `"port=5432 dbname=mydb"`) |
| `pg_conn()` | int32 | Get last connection handle |
| `pg_exec(conn, sql)` | int32 | Execute SQL (no params) |
| `pg_param_add_text/int/double/null()` | int32 | Stage parameters |
| `pg_query(conn, sql)` | int32 | Parameterized SELECT |
| `pg_execute(conn, sql)` | int32 | Parameterized INSERT/UPDATE/DELETE |
| `pg_next_row(conn)` | int32 | Advance to next row |
| `pg_begin/commit/rollback(conn)` | int32 | Transaction control |
| `pg_listen(conn, channel)` | int32 | Subscribe to notifications |
| `pg_notify(conn, channel, payload)` | int32 | Send notification |
| `pg_disconnect(conn)` | int32 | Disconnect |

## MySQL API Reference

| Function | Returns | Description |
|----------|---------|-------------|
| `mysql_db_connect(host, user, pw, db, port)` | int32 | Connect |
| `mysql_db_conn()` | int32 | Get last connection handle |
| `mysql_db_exec(conn, sql)` | int32 | Execute SQL (no params) |
| `mysql_db_param_add_text/int/double/null()` | int32 | Stage parameters |
| `mysql_db_execute(conn, sql)` | int32 | Parameterized DML |
| `mysql_db_select(conn, sql)` | int32 | Parameterized SELECT |
| `mysql_db_simple_query(conn, sql)` | int32 | Non-parameterized SELECT |
| `mysql_db_next_row(conn)` | int32 | Advance to next row |
| `mysql_db_affected_rows(conn)` | int64 | Rows affected by last DML |
| `mysql_db_last_insert_id(conn)` | int64 | Auto-increment ID |
| `mysql_db_begin/commit/rollback(conn)` | int32 | Transaction control |
| `mysql_db_disconnect(conn)` | int32 | Disconnect |

## Redis API Reference

| Function | Returns | Description |
|----------|---------|-------------|
| `redis_connect(host, port)` | int32 | Connect |
| `redis_connect_auth(host, port, pw)` | int32 | Connect with AUTH |
| `redis_conn()` | int32 | Get last connection handle |
| `redis_set/get/del(conn, key, ...)` | int32 | Key-value operations |
| `redis_exists(conn, key)` | int32 | Check key existence |
| `redis_expire(conn, key, secs)` | int32 | Set key expiry |
| `redis_ttl(conn, key)` | int64 | Get remaining TTL |
| `redis_incr/incrby(conn, key, ...)` | int32 | Atomic increment |
| `redis_lpush/rpush/lpop/rpop(conn, key, ...)` | int32 | List operations |
| `redis_llen(conn, key)` | int64 | List length |
| `redis_lrange(conn, key, start, stop)` | int32 | List range query |
| `redis_hset/hget/hdel(conn, key, field, ...)` | int32 | Hash operations |
| `redis_sadd/srem/sismember(conn, key, member)` | int32 | Set operations |
| `redis_ping(conn)` | int32 | Ping server |
| `redis_select_db(conn, db)` | int32 | Select database number |
| `redis_command(conn, cmd)` | int32 | Execute raw command string |
| `redis_disconnect(conn)` | int32 | Disconnect |

## Known Compiler Workarounds

These workarounds are required due to known Aria compiler bugs (as of v0.2.2):

1. **Move semantics**: All variables are consumed on first use. Use `last_conn()` / `last_db()` before each call that needs a handle.

2. **Reserved names in extern**: `close` and `free` in function names cause compiler issues. All drivers use `disconnect` instead.

3. **String return corruption**: Strings returned from extern functions may be corrupted. All drivers provide C-side assertion helpers for testing instead of returning strings to Aria.

4. **Function call limits in main**: Split test logic into phase functions called from main with `drop(test_phaseN());`.

5. **void vs NIL**: Aria functions must return `NIL` (not `void`). `void` is only valid in extern declarations.
