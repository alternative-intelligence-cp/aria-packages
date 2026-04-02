/*
 * aria_libc_net — Networking (TCP/UDP sockets) for Aria
 * Build: cc -O2 -shared -fPIC -o libaria_libc_net.so aria_libc_net.c
 */
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

/* ── Socket Creation ─────────────────────────────────────────────── */

int64_t aria_libc_net_socket_tcp(void) {
    return (int64_t)socket(AF_INET, SOCK_STREAM, 0);
}

int64_t aria_libc_net_socket_udp(void) {
    return (int64_t)socket(AF_INET, SOCK_DGRAM, 0);
}

int32_t aria_libc_net_close(int64_t fd) {
    if (fd < 0) return -1;
    return (int32_t)close((int)fd);
}

/* ── Server ──────────────────────────────────────────────────────── */

int32_t aria_libc_net_bind(int64_t fd, const char *addr, int32_t port) {
    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons((uint16_t)port);
    if (!addr || strcmp(addr, "0.0.0.0") == 0)
        sa.sin_addr.s_addr = INADDR_ANY;
    else
        inet_pton(AF_INET, addr, &sa.sin_addr);
    return (int32_t)bind((int)fd, (struct sockaddr *)&sa, sizeof(sa));
}

int32_t aria_libc_net_listen(int64_t fd, int32_t backlog) {
    return (int32_t)listen((int)fd, backlog);
}

int64_t aria_libc_net_accept(int64_t fd) {
    struct sockaddr_in client;
    socklen_t len = sizeof(client);
    return (int64_t)accept((int)fd, (struct sockaddr *)&client, &len);
}

int32_t aria_libc_net_setsockopt_reuse(int64_t fd) {
    int opt = 1;
    return (int32_t)setsockopt((int)fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
}

/* ── Client ──────────────────────────────────────────────────────── */

int32_t aria_libc_net_connect(int64_t fd, const char *addr, int32_t port) {
    if (!addr) return -1;
    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons((uint16_t)port);
    inet_pton(AF_INET, addr, &sa.sin_addr);
    return (int32_t)connect((int)fd, (struct sockaddr *)&sa, sizeof(sa));
}

/* ── Send / Recv ─────────────────────────────────────────────────── */

int64_t aria_libc_net_send(int64_t fd, const char *data, int64_t len) {
    if (fd < 0 || !data) return -1;
    return (int64_t)send((int)fd, data, (size_t)len, 0);
}

int64_t aria_libc_net_send_str(int64_t fd, const char *s) {
    if (fd < 0 || !s) return -1;
    return (int64_t)send((int)fd, s, strlen(s), 0);
}

static char g_recv_buf[65536];

const char *aria_libc_net_recv(int64_t fd, int64_t maxlen) {
    g_recv_buf[0] = '\0';
    if (fd < 0) return g_recv_buf;
    if (maxlen <= 0 || (size_t)maxlen >= sizeof(g_recv_buf))
        maxlen = sizeof(g_recv_buf) - 1;
    ssize_t n = recv((int)fd, g_recv_buf, (size_t)maxlen, 0);
    if (n <= 0) { g_recv_buf[0] = '\0'; return g_recv_buf; }
    g_recv_buf[n] = '\0';
    return g_recv_buf;
}

int64_t aria_libc_net_recv_raw(int64_t fd, int64_t buf, int64_t maxlen) {
    if (fd < 0 || !buf || maxlen <= 0) return -1;
    return (int64_t)recv((int)fd, (void *)(uintptr_t)buf, (size_t)maxlen, 0);
}

/* ── DNS Lookup ──────────────────────────────────────────────────── */

static char g_ip_buf[INET6_ADDRSTRLEN + 1];

const char *aria_libc_net_resolve(const char *hostname) {
    g_ip_buf[0] = '\0';
    if (!hostname) return g_ip_buf;
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(hostname, NULL, &hints, &res) != 0) return g_ip_buf;
    struct sockaddr_in *sa = (struct sockaddr_in *)res->ai_addr;
    inet_ntop(AF_INET, &sa->sin_addr, g_ip_buf, sizeof(g_ip_buf));
    freeaddrinfo(res);
    return g_ip_buf;
}

/* ── Non-blocking ────────────────────────────────────────────────── */

int32_t aria_libc_net_set_nonblocking(int64_t fd) {
    int flags = fcntl((int)fd, F_GETFL, 0);
    if (flags < 0) return -1;
    return (int32_t)fcntl((int)fd, F_SETFL, flags | O_NONBLOCK);
}

int32_t aria_libc_net_set_blocking(int64_t fd) {
    int flags = fcntl((int)fd, F_GETFL, 0);
    if (flags < 0) return -1;
    return (int32_t)fcntl((int)fd, F_SETFL, flags & ~O_NONBLOCK);
}

/* ── Error ───────────────────────────────────────────────────────── */

int32_t aria_libc_net_errno(void) { return (int32_t)errno; }

const char *aria_libc_net_strerror(int32_t err) {
    return strerror(err);
}
