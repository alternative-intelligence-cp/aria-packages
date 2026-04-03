/*
 * aria_libc_net — Networking shim for Aria (TCP, UDP, DNS, poll)
 * Build: cc -O2 -shared -fPIC -o libaria_libc_net.so aria_libc_net.c
 *
 * Handle-based API: socket FDs passed as int64.
 * All string params are const char* (null-terminated).
 * All string returns use AriaString {char*, int64_t}.
 *
 * v0.12.0: Extended with poll, timeout, sendto/recvfrom, getpeername for networking stack.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <poll.h>

/* ════════════════════════════════════════════════════════════════════════════
 * AriaString return type: {char* data, int64_t length}
 * Returned in %rax (data ptr) and %rdx (length).
 * ════════════════════════════════════════════════════════════════════════════ */
typedef struct {
    const char *data;
    int64_t     length;
} AriaString;

/* ════════════════════════════════════════════════════════════════════════════
 * Static buffers for recv/resolve results
 * ════════════════════════════════════════════════════════════════════════════ */
#define RECV_BUF_SIZE 65536
#define RESOLVE_BUF   256
#define ERR_BUF       512

static char g_recv_buf[RECV_BUF_SIZE];
static int64_t g_recv_len = 0;
static char g_resolve_buf[RESOLVE_BUF];
static char g_err_buf[ERR_BUF] = "";
static char g_peer_buf[128] = "";

/* ════════════════════════════════════════════════════════════════════════════
 * Socket creation
 * ════════════════════════════════════════════════════════════════════════════ */

int64_t aria_libc_net_socket_tcp(void) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    return (int64_t)fd;
}

int64_t aria_libc_net_socket_udp(void) {
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    return (int64_t)fd;
}

/* ════════════════════════════════════════════════════════════════════════════
 * Socket options
 * ════════════════════════════════════════════════════════════════════════════ */

int64_t aria_libc_net_setsockopt_reuse(int64_t fd) {
    int yes = 1;
    int rv = setsockopt((int)fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    return (rv == 0) ? 1 : 0;
}

int64_t aria_libc_net_set_nonblocking(int64_t fd) {
    int flags = fcntl((int)fd, F_GETFL, 0);
    if (flags < 0) return 0;
    if (fcntl((int)fd, F_SETFL, flags | O_NONBLOCK) < 0) return 0;
    return 1;
}

int64_t aria_libc_net_set_blocking(int64_t fd) {
    int flags = fcntl((int)fd, F_GETFL, 0);
    if (flags < 0) return 0;
    if (fcntl((int)fd, F_SETFL, flags & ~O_NONBLOCK) < 0) return 0;
    return 1;
}

/* ════════════════════════════════════════════════════════════════════════════
 * Bind / Listen / Accept / Connect
 * ════════════════════════════════════════════════════════════════════════════ */

int64_t aria_libc_net_bind(int64_t fd, const char *addr, int64_t port) {
    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family      = AF_INET;
    sa.sin_port        = htons((uint16_t)port);
    sa.sin_addr.s_addr = inet_addr(addr);
    int rv = bind((int)fd, (struct sockaddr *)&sa, sizeof(sa));
    return (rv == 0) ? 1 : 0;
}

int64_t aria_libc_net_listen(int64_t fd, int64_t backlog) {
    int rv = listen((int)fd, (int)backlog);
    return (rv == 0) ? 1 : 0;
}

int64_t aria_libc_net_accept(int64_t fd) {
    struct sockaddr_in client;
    socklen_t clen = sizeof(client);
    int cfd = accept((int)fd, (struct sockaddr *)&client, &clen);
    return (int64_t)cfd;
}

int64_t aria_libc_net_connect(int64_t fd, const char *host, int64_t port) {
    struct addrinfo hints, *res, *p;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%ld", (long)port);

    int rv = getaddrinfo(host, port_str, &hints, &res);
    if (rv != 0) return -1;

    int connected = -1;
    for (p = res; p; p = p->ai_next) {
        if (connect((int)fd, p->ai_addr, p->ai_addrlen) == 0) {
            connected = 0;
            break;
        }
    }
    freeaddrinfo(res);
    return (connected == 0) ? 1 : 0;
}

/* ════════════════════════════════════════════════════════════════════════════
 * Send / Recv (TCP)
 * ════════════════════════════════════════════════════════════════════════════ */

int64_t aria_libc_net_send(int64_t fd, const char *data, int64_t len) {
    ssize_t n = send((int)fd, data, (size_t)len, 0);
    return (int64_t)n;
}

int64_t aria_libc_net_send_str(int64_t fd, const char *data) {
    ssize_t n = send((int)fd, data, strlen(data), 0);
    return (int64_t)n;
}

AriaString aria_libc_net_recv(int64_t fd, int64_t max_len) {
    int64_t cap = max_len;
    if (cap > RECV_BUF_SIZE - 1) cap = RECV_BUF_SIZE - 1;
    ssize_t n = recv((int)fd, g_recv_buf, (size_t)cap, 0);
    if (n < 0) { g_recv_buf[0] = '\0'; g_recv_len = 0; }
    else { g_recv_buf[n] = '\0'; g_recv_len = (int64_t)n; }
    return (AriaString){ g_recv_buf, g_recv_len };
}

int64_t aria_libc_net_recv_raw(int64_t fd, int64_t buf_ptr, int64_t max_len) {
    ssize_t n = recv((int)fd, (void *)buf_ptr, (size_t)max_len, 0);
    return (int64_t)n;
}

/* ════════════════════════════════════════════════════════════════════════════
 * UDP sendto / recvfrom  (NEW v0.12.0)
 * ════════════════════════════════════════════════════════════════════════════ */

int64_t aria_libc_net_sendto(int64_t fd, const char *data, int64_t len,
                             const char *host, int64_t port) {
    struct sockaddr_in dest;
    memset(&dest, 0, sizeof(dest));
    dest.sin_family      = AF_INET;
    dest.sin_port        = htons((uint16_t)port);
    dest.sin_addr.s_addr = inet_addr(host);
    ssize_t n = sendto((int)fd, data, (size_t)len, 0,
                       (struct sockaddr *)&dest, sizeof(dest));
    return (int64_t)n;
}

AriaString aria_libc_net_recvfrom(int64_t fd) {
    struct sockaddr_in src;
    socklen_t slen = sizeof(src);
    ssize_t n = recvfrom((int)fd, g_recv_buf, RECV_BUF_SIZE - 1, 0,
                         (struct sockaddr *)&src, &slen);
    if (n < 0) { g_recv_buf[0] = '\0'; g_recv_len = 0; }
    else { g_recv_buf[n] = '\0'; g_recv_len = (int64_t)n; }
    return (AriaString){ g_recv_buf, g_recv_len };
}

/* ════════════════════════════════════════════════════════════════════════════
 * Poll / Timeout  (NEW v0.12.0)
 * ════════════════════════════════════════════════════════════════════════════ */

int64_t aria_libc_net_poll_read(int64_t fd, int64_t timeout_ms) {
    struct pollfd pfd;
    pfd.fd = (int)fd;
    pfd.events = POLLIN;
    pfd.revents = 0;
    int rv = poll(&pfd, 1, (int)timeout_ms);
    if (rv < 0) return -1;
    if (rv == 0) return 0;
    return 1;
}

int64_t aria_libc_net_poll_write(int64_t fd, int64_t timeout_ms) {
    struct pollfd pfd;
    pfd.fd = (int)fd;
    pfd.events = POLLOUT;
    pfd.revents = 0;
    int rv = poll(&pfd, 1, (int)timeout_ms);
    if (rv < 0) return -1;
    if (rv == 0) return 0;
    return 1;
}

int64_t aria_libc_net_set_timeout(int64_t fd, int64_t seconds) {
    struct timeval tv;
    tv.tv_sec  = (time_t)seconds;
    tv.tv_usec = 0;
    if (setsockopt((int)fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) return 0;
    if (setsockopt((int)fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0) return 0;
    return 1;
}

/* ════════════════════════════════════════════════════════════════════════════
 * DNS resolve
 * ════════════════════════════════════════════════════════════════════════════ */

AriaString aria_libc_net_resolve(const char *hostname) {
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    int rv = getaddrinfo(hostname, NULL, &hints, &res);
    if (rv != 0) {
        g_resolve_buf[0] = '\0';
        return (AriaString){ g_resolve_buf, 0 };
    }

    struct sockaddr_in *ipv4 = (struct sockaddr_in *)res->ai_addr;
    const char *ip = inet_ntop(AF_INET, &ipv4->sin_addr, g_resolve_buf, RESOLVE_BUF);
    int64_t len = 0;
    if (ip) len = (int64_t)strlen(ip);
    else g_resolve_buf[0] = '\0';
    freeaddrinfo(res);
    return (AriaString){ g_resolve_buf, len };
}

/* Reverse DNS: IP → hostname */
static char g_reverse_buf[256] = "";

AriaString aria_libc_net_reverse_lookup(const char *ip_addr) {
    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    inet_pton(AF_INET, ip_addr, &sa.sin_addr);

    int rv = getnameinfo((struct sockaddr *)&sa, sizeof(sa),
                         g_reverse_buf, sizeof(g_reverse_buf),
                         NULL, 0, 0);
    if (rv != 0) {
        g_reverse_buf[0] = '\0';
        return (AriaString){ g_reverse_buf, 0 };
    }
    return (AriaString){ g_reverse_buf, (int64_t)strlen(g_reverse_buf) };
}

/* Check if string is a valid IPv4 address */
int64_t aria_libc_net_is_ipv4(const char *addr) {
    struct in_addr ipv4;
    return (inet_pton(AF_INET, addr, &ipv4) == 1) ? 1 : 0;
}

/* Resolve all addresses (comma-separated string, max 8 results) */
#define RESOLVE_ALL_BUF 512
static char g_resolve_all_buf[RESOLVE_ALL_BUF];

AriaString aria_libc_net_resolve_all(const char *hostname) {
    struct addrinfo hints, *res, *p;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    int rv = getaddrinfo(hostname, NULL, &hints, &res);
    if (rv != 0) {
        g_resolve_all_buf[0] = '\0';
        return (AriaString){ g_resolve_all_buf, 0 };
    }

    g_resolve_all_buf[0] = '\0';
    int count = 0;
    char tmp[64];
    for (p = res; p && count < 8; p = p->ai_next) {
        struct sockaddr_in *ipv4 = (struct sockaddr_in *)p->ai_addr;
        if (inet_ntop(AF_INET, &ipv4->sin_addr, tmp, sizeof(tmp))) {
            if (count > 0) strncat(g_resolve_all_buf, ",", RESOLVE_ALL_BUF - strlen(g_resolve_all_buf) - 1);
            strncat(g_resolve_all_buf, tmp, RESOLVE_ALL_BUF - strlen(g_resolve_all_buf) - 1);
            count++;
        }
    }
    freeaddrinfo(res);
    return (AriaString){ g_resolve_all_buf, (int64_t)strlen(g_resolve_all_buf) };
}

/* ════════════════════════════════════════════════════════════════════════════
 * Peer info  (NEW v0.12.0)
 * ════════════════════════════════════════════════════════════════════════════ */

AriaString aria_libc_net_getpeername(int64_t fd) {
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    if (getpeername((int)fd, (struct sockaddr *)&addr, &len) < 0) {
        g_peer_buf[0] = '\0';
        return (AriaString){ g_peer_buf, 0 };
    }
    const char *ip = inet_ntop(AF_INET, &addr.sin_addr, g_peer_buf, sizeof(g_peer_buf));
    int64_t slen = ip ? (int64_t)strlen(ip) : 0;
    return (AriaString){ g_peer_buf, slen };
}

int64_t aria_libc_net_getpeerport(int64_t fd) {
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    if (getpeername((int)fd, (struct sockaddr *)&addr, &len) < 0) return -1;
    return (int64_t)ntohs(addr.sin_port);
}

/* ════════════════════════════════════════════════════════════════════════════
 * Close / Error
 * ════════════════════════════════════════════════════════════════════════════ */

int64_t aria_libc_net_close(int64_t fd) {
    if (fd >= 0) close((int)fd);
    return 0;
}

int64_t aria_libc_net_errno(void) {
    return (int64_t)errno;
}

AriaString aria_libc_net_strerror(void) {
    const char *msg = strerror(errno);
    return (AriaString){ msg, (int64_t)strlen(msg) };
}

/* ════════════════════════════════════════════════════════════════════════════
 * Recv length accessor
 * ════════════════════════════════════════════════════════════════════════════ */

int64_t aria_libc_net_recv_length(void) {
    return g_recv_len;
}
