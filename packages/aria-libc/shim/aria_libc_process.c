/*
 * aria_libc_process — Process management and signals for Aria
 * Build: cc -O2 -shared -fPIC -o libaria_libc_process.so aria_libc_process.c
 */
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>

/* ── Process Info ────────────────────────────────────────────────── */

int64_t aria_libc_process_getpid(void)  { return (int64_t)getpid(); }
int64_t aria_libc_process_getppid(void) { return (int64_t)getppid(); }
int32_t aria_libc_process_getuid(void)  { return (int32_t)getuid(); }
int32_t aria_libc_process_getgid(void)  { return (int32_t)getgid(); }

/* ── Fork / Exec / Wait ─────────────────────────────────────────── */

int64_t aria_libc_process_fork(void) {
    return (int64_t)fork();
}

int32_t aria_libc_process_execvp(const char *file, const char *arg0,
                                  const char *arg1, const char *arg2) {
    /* Max 4-arg exec for safety. NULL-terminate the array. */
    const char *argv[5];
    int argc = 0;
    if (arg0) argv[argc++] = arg0;
    if (arg1) argv[argc++] = arg1;
    if (arg2) argv[argc++] = arg2;
    argv[argc] = NULL;
    return (int32_t)execvp(file, (char *const *)argv);
}

int32_t aria_libc_process_waitpid(int64_t pid) {
    int status = 0;
    waitpid((pid_t)pid, &status, 0);
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    return -1;
}

/* ── System ──────────────────────────────────────────────────────── */

int32_t aria_libc_process_system(const char *cmd) {
    if (!cmd) return -1;
    return WEXITSTATUS(system(cmd));
}

/* ── Signal ──────────────────────────────────────────────────────── */

int32_t aria_libc_process_kill(int64_t pid, int32_t sig) {
    return (int32_t)kill((pid_t)pid, sig);
}

int32_t aria_libc_process_raise(int32_t sig) {
    return (int32_t)raise(sig);
}

/* Signal constants */
int32_t aria_libc_process_SIGTERM(void) { return SIGTERM; }
int32_t aria_libc_process_SIGKILL(void) { return SIGKILL; }
int32_t aria_libc_process_SIGINT(void)  { return SIGINT; }
int32_t aria_libc_process_SIGHUP(void)  { return SIGHUP; }
int32_t aria_libc_process_SIGUSR1(void) { return SIGUSR1; }
int32_t aria_libc_process_SIGUSR2(void) { return SIGUSR2; }
int32_t aria_libc_process_SIGCHLD(void) { return SIGCHLD; }

/* ── Pipe ────────────────────────────────────────────────────────── */

static int g_pipe_fds[2];

int32_t aria_libc_process_pipe(void) {
    return (int32_t)pipe(g_pipe_fds);
}

int64_t aria_libc_process_pipe_read_fd(void)  { return (int64_t)g_pipe_fds[0]; }
int64_t aria_libc_process_pipe_write_fd(void) { return (int64_t)g_pipe_fds[1]; }

/* ── Dup ─────────────────────────────────────────────────────────── */

int32_t aria_libc_process_dup2(int64_t oldfd, int64_t newfd) {
    return (int32_t)dup2((int)oldfd, (int)newfd);
}

/* ── Environment ─────────────────────────────────────────────────── */

static char g_cwd_buf[4096];

const char *aria_libc_process_getcwd(void) {
    g_cwd_buf[0] = '\0';
    char *r = getcwd(g_cwd_buf, sizeof(g_cwd_buf));
    if (!r) g_cwd_buf[0] = '\0';
    return g_cwd_buf;
}

int32_t aria_libc_process_chdir(const char *path) {
    if (!path) return -1;
    return (int32_t)chdir(path);
}
