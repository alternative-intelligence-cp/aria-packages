/* aria_env_shim.c — Environment variable management */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

extern char **environ;

/* ---- get env var ---- */
const char *aria_env_get(const char *name) {
    const char *val = getenv(name);
    return val ? val : "";
}

/* ---- set env var ---- */
int32_t aria_env_set(const char *name, const char *value) {
    return setenv(name, value, 1) == 0 ? 1 : 0;
}

/* ---- unset env var ---- */
int32_t aria_env_unset(const char *name) {
    return unsetenv(name) == 0 ? 1 : 0;
}

/* ---- has env var ---- */
int32_t aria_env_has(const char *name) {
    return getenv(name) != NULL ? 1 : 0;
}

/* ---- get with default ---- */
const char *aria_env_get_or(const char *name, const char *default_val) {
    const char *val = getenv(name);
    return val ? val : default_val;
}

/* ---- count env vars ---- */
int32_t aria_env_count(void) {
    int32_t n = 0;
    for (char **e = environ; *e; e++) n++;
    return n;
}

/* ---- get HOME ---- */
const char *aria_env_home(void) {
    const char *h = getenv("HOME");
    return h ? h : "";
}

/* ---- get PATH ---- */
const char *aria_env_path(void) {
    const char *p = getenv("PATH");
    return p ? p : "";
}

/* ---- get USER ---- */
const char *aria_env_user(void) {
    const char *u = getenv("USER");
    if (!u) u = getenv("LOGNAME");
    return u ? u : "";
}

/* ---- get SHELL ---- */
const char *aria_env_shell(void) {
    const char *s = getenv("SHELL");
    return s ? s : "";
}
