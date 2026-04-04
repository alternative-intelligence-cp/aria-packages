/*
 * aria_libc_server — HTTP/1.1 server shim for Aria
 * Build: cc -O2 -shared -fPIC -o libaria_libc_server.so aria_libc_server.c
 *
 * Provides HTTP request parsing and response building on top of POSIX sockets.
 * Designed for single-threaded use (one request at a time).
 *
 * v0.12.0: New — replaces old aria_server_shim with aria-libc-based implementation.
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
#include <fcntl.h>

/* ════════════════════════════════════════════════════════════════════════════
 * AriaString return type
 * ════════════════════════════════════════════════════════════════════════════ */
typedef struct {
    const char *data;
    int64_t     length;
} AriaString;

/* ════════════════════════════════════════════════════════════════════════════
 * Request parsing limits and storage
 * ════════════════════════════════════════════════════════════════════════════ */
#define REQ_BUF_SIZE     65536
#define MAX_HEADERS      64
#define HEADER_NAME_LEN  256
#define HEADER_VAL_LEN   2048
#define METHOD_LEN       16
#define PATH_LEN         4096
#define QUERY_LEN        4096
#define VERSION_LEN      16
#define RESP_BUF_SIZE    65536

/* parsed request fields */
static char g_req_buf[REQ_BUF_SIZE];
static int64_t g_req_len = 0;

static char g_method[METHOD_LEN]   = "";
static char g_path[PATH_LEN]       = "";
static char g_query[QUERY_LEN]     = "";
static char g_version[VERSION_LEN] = "";

static char g_hdr_names[MAX_HEADERS][HEADER_NAME_LEN];
static char g_hdr_values[MAX_HEADERS][HEADER_VAL_LEN];
static int  g_hdr_count = 0;

static char *g_body_ptr = NULL;
static int64_t g_body_len = 0;

/* response buffer */
static char g_resp_buf[RESP_BUF_SIZE];

/* error buffer */
static char g_err_buf[512] = "";

/* helper return buffer */
static char g_ret_buf[HEADER_VAL_LEN] = "";

/* peer info */
static char g_peer_addr[64] = "";
static int64_t g_peer_port = 0;

/* ════════════════════════════════════════════════════════════════════════════
 * Internal: parse raw HTTP request from buffer
 * ════════════════════════════════════════════════════════════════════════════ */
static void parse_request(void) {
    g_method[0]  = '\0';
    g_path[0]    = '\0';
    g_query[0]   = '\0';
    g_version[0] = '\0';
    g_hdr_count  = 0;
    g_body_ptr   = NULL;
    g_body_len   = 0;

    if (g_req_len <= 0) return;

    char *buf = g_req_buf;
    char *end = buf + g_req_len;

    /* ── request line: METHOD SP PATH SP VERSION CRLF ── */
    char *line_end = strstr(buf, "\r\n");
    if (!line_end) line_end = strstr(buf, "\n");
    if (!line_end) return;

    /* method */
    char *sp1 = memchr(buf, ' ', (size_t)(line_end - buf));
    if (!sp1) return;
    int64_t mlen = sp1 - buf;
    if (mlen >= METHOD_LEN) mlen = METHOD_LEN - 1;
    memcpy(g_method, buf, (size_t)mlen);
    g_method[mlen] = '\0';

    /* path (+ optional query) */
    char *path_start = sp1 + 1;
    char *sp2 = memchr(path_start, ' ', (size_t)(line_end - path_start));
    if (!sp2) return;

    /* check for query string */
    char *qmark = memchr(path_start, '?', (size_t)(sp2 - path_start));
    if (qmark) {
        int64_t plen = qmark - path_start;
        if (plen >= PATH_LEN) plen = PATH_LEN - 1;
        memcpy(g_path, path_start, (size_t)plen);
        g_path[plen] = '\0';

        int64_t qlen = sp2 - (qmark + 1);
        if (qlen >= QUERY_LEN) qlen = QUERY_LEN - 1;
        memcpy(g_query, qmark + 1, (size_t)qlen);
        g_query[qlen] = '\0';
    } else {
        int64_t plen = sp2 - path_start;
        if (plen >= PATH_LEN) plen = PATH_LEN - 1;
        memcpy(g_path, path_start, (size_t)plen);
        g_path[plen] = '\0';
        g_query[0] = '\0';
    }

    /* version */
    char *ver_start = sp2 + 1;
    int64_t vlen = line_end - ver_start;
    if (vlen >= VERSION_LEN) vlen = VERSION_LEN - 1;
    memcpy(g_version, ver_start, (size_t)vlen);
    g_version[vlen] = '\0';

    /* advance past request line */
    char *cur = line_end;
    if (cur[0] == '\r' && cur[1] == '\n') cur += 2;
    else if (cur[0] == '\n') cur += 1;

    /* ── headers ── */
    while (cur < end && g_hdr_count < MAX_HEADERS) {
        /* empty line = end of headers */
        if (cur[0] == '\r' && cur + 1 < end && cur[1] == '\n') {
            cur += 2;
            break;
        }
        if (cur[0] == '\n') {
            cur += 1;
            break;
        }

        char *hdr_end = strstr(cur, "\r\n");
        if (!hdr_end) hdr_end = strstr(cur, "\n");
        if (!hdr_end) break;

        /* find colon separator */
        char *colon = memchr(cur, ':', (size_t)(hdr_end - cur));
        if (colon) {
            int64_t nlen = colon - cur;
            if (nlen >= HEADER_NAME_LEN) nlen = HEADER_NAME_LEN - 1;
            memcpy(g_hdr_names[g_hdr_count], cur, (size_t)nlen);
            g_hdr_names[g_hdr_count][nlen] = '\0';

            /* skip ": " */
            char *val_start = colon + 1;
            while (val_start < hdr_end && *val_start == ' ') val_start++;

            int64_t vl = hdr_end - val_start;
            if (vl >= HEADER_VAL_LEN) vl = HEADER_VAL_LEN - 1;
            memcpy(g_hdr_values[g_hdr_count], val_start, (size_t)vl);
            g_hdr_values[g_hdr_count][vl] = '\0';

            g_hdr_count++;
        }

        cur = hdr_end;
        if (cur[0] == '\r' && cur + 1 < end && cur[1] == '\n') cur += 2;
        else if (cur[0] == '\n') cur += 1;
    }

    /* ── body (everything after blank line) ── */
    if (cur < end) {
        g_body_ptr = cur;
        g_body_len = (int64_t)(end - cur);
    }
}

/* ════════════════════════════════════════════════════════════════════════════
 * Server lifecycle
 * ════════════════════════════════════════════════════════════════════════════ */

int64_t aria_libc_server_listen(const char *addr, int64_t port, int64_t backlog) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        snprintf(g_err_buf, sizeof(g_err_buf), "socket: %s", strerror(errno));
        return -1;
    }

    int yes = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family      = AF_INET;
    sa.sin_port        = htons((uint16_t)port);
    sa.sin_addr.s_addr = inet_addr(addr);

    if (bind(fd, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
        snprintf(g_err_buf, sizeof(g_err_buf), "bind: %s", strerror(errno));
        close(fd);
        return -1;
    }

    if (listen(fd, (int)backlog) < 0) {
        snprintf(g_err_buf, sizeof(g_err_buf), "listen: %s", strerror(errno));
        close(fd);
        return -1;
    }

    return (int64_t)fd;
}

int64_t aria_libc_server_accept(int64_t server_fd) {
    struct sockaddr_in client;
    socklen_t clen = sizeof(client);
    int cfd = accept((int)server_fd, (struct sockaddr *)&client, &clen);
    if (cfd < 0) {
        snprintf(g_err_buf, sizeof(g_err_buf), "accept: %s", strerror(errno));
        return -1;
    }

    /* store peer info */
    inet_ntop(AF_INET, &client.sin_addr, g_peer_addr, sizeof(g_peer_addr));
    g_peer_port = (int64_t)ntohs(client.sin_port);

    return (int64_t)cfd;
}

int64_t aria_libc_server_close(int64_t fd) {
    return (close((int)fd) == 0) ? 1 : 0;
}

int64_t aria_libc_server_close_client(int64_t client_fd) {
    return (close((int)client_fd) == 0) ? 1 : 0;
}

AriaString aria_libc_server_error(void) {
    return (AriaString){ g_err_buf, (int64_t)strlen(g_err_buf) };
}

/* ════════════════════════════════════════════════════════════════════════════
 * Read + parse request from client fd
 * ════════════════════════════════════════════════════════════════════════════ */

int64_t aria_libc_server_read_request(int64_t client_fd) {
    g_req_len = 0;
    g_req_buf[0] = '\0';

    /* read in a loop until we see \r\n\r\n (end of headers) or buffer full */
    int64_t total = 0;
    while (total < REQ_BUF_SIZE - 1) {
        ssize_t n = recv((int)client_fd, g_req_buf + total,
                         (size_t)(REQ_BUF_SIZE - 1 - total), 0);
        if (n <= 0) break;
        total += n;
        g_req_buf[total] = '\0';

        /* check for end of headers */
        if (strstr(g_req_buf, "\r\n\r\n")) {
            /* check Content-Length for body */
            char *cl = strcasestr(g_req_buf, "content-length:");
            if (cl) {
                char *val = cl + 15;
                while (*val == ' ') val++;
                int64_t body_expected = strtol(val, NULL, 10);

                /* find body start */
                char *body_start = strstr(g_req_buf, "\r\n\r\n") + 4;
                int64_t body_have = total - (int64_t)(body_start - g_req_buf);

                /* read remaining body bytes */
                while (body_have < body_expected && total < REQ_BUF_SIZE - 1) {
                    ssize_t m = recv((int)client_fd, g_req_buf + total,
                                     (size_t)(REQ_BUF_SIZE - 1 - total), 0);
                    if (m <= 0) break;
                    total += m;
                    body_have += m;
                    g_req_buf[total] = '\0';
                }
            }
            break;
        }
    }

    g_req_len = total;
    g_req_buf[total] = '\0';

    if (total <= 0) {
        snprintf(g_err_buf, sizeof(g_err_buf), "recv: empty request");
        return 0;
    }

    parse_request();
    return 1;
}

/* ════════════════════════════════════════════════════════════════════════════
 * Request accessors (after read_request)
 * ════════════════════════════════════════════════════════════════════════════ */

AriaString aria_libc_server_req_method(void) {
    return (AriaString){ g_method, (int64_t)strlen(g_method) };
}

AriaString aria_libc_server_req_path(void) {
    return (AriaString){ g_path, (int64_t)strlen(g_path) };
}

AriaString aria_libc_server_req_query(void) {
    return (AriaString){ g_query, (int64_t)strlen(g_query) };
}

AriaString aria_libc_server_req_version(void) {
    return (AriaString){ g_version, (int64_t)strlen(g_version) };
}

AriaString aria_libc_server_req_body(void) {
    if (!g_body_ptr) return (AriaString){ "", 0 };
    return (AriaString){ g_body_ptr, g_body_len };
}

int64_t aria_libc_server_req_body_length(void) {
    return g_body_len;
}

/* get header value by name (case-insensitive) */
AriaString aria_libc_server_req_header(const char *name) {
    for (int i = 0; i < g_hdr_count; i++) {
        if (strcasecmp(g_hdr_names[i], name) == 0) {
            return (AriaString){ g_hdr_values[i],
                                 (int64_t)strlen(g_hdr_values[i]) };
        }
    }
    return (AriaString){ "", 0 };
}

int64_t aria_libc_server_req_header_count(void) {
    return (int64_t)g_hdr_count;
}

/* get header name by index */
AriaString aria_libc_server_req_header_name(int64_t idx) {
    if (idx < 0 || idx >= g_hdr_count) return (AriaString){ "", 0 };
    return (AriaString){ g_hdr_names[idx],
                         (int64_t)strlen(g_hdr_names[idx]) };
}

/* get header value by index */
AriaString aria_libc_server_req_header_value(int64_t idx) {
    if (idx < 0 || idx >= g_hdr_count) return (AriaString){ "", 0 };
    return (AriaString){ g_hdr_values[idx],
                         (int64_t)strlen(g_hdr_values[idx]) };
}

/* ════════════════════════════════════════════════════════════════════════════
 * Peer info (from last accept)
 * ════════════════════════════════════════════════════════════════════════════ */

AriaString aria_libc_server_peer_addr(void) {
    return (AriaString){ g_peer_addr, (int64_t)strlen(g_peer_addr) };
}

int64_t aria_libc_server_peer_port(void) {
    return g_peer_port;
}

/* ════════════════════════════════════════════════════════════════════════════
 * Send HTTP response
 * ════════════════════════════════════════════════════════════════════════════ */

static const char *status_text(int64_t code) {
    switch (code) {
        case 200: return "OK";
        case 201: return "Created";
        case 204: return "No Content";
        case 301: return "Moved Permanently";
        case 302: return "Found";
        case 304: return "Not Modified";
        case 400: return "Bad Request";
        case 401: return "Unauthorized";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 408: return "Request Timeout";
        case 413: return "Payload Too Large";
        case 429: return "Too Many Requests";
        case 500: return "Internal Server Error";
        case 502: return "Bad Gateway";
        case 503: return "Service Unavailable";
        default:  return "Unknown";
    }
}

int64_t aria_libc_server_respond(int64_t client_fd, int64_t status_code,
                                  const char *body) {
    int64_t body_len = (int64_t)strlen(body);
    int written = snprintf(g_resp_buf, RESP_BUF_SIZE,
        "HTTP/1.1 %ld %s\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: %ld\r\n"
        "Connection: close\r\n"
        "\r\n"
        "%s",
        (long)status_code, status_text(status_code),
        (long)body_len, body);

    if (written <= 0) return 0;
    ssize_t n = send((int)client_fd, g_resp_buf, (size_t)written, 0);
    return (n > 0) ? 1 : 0;
}

int64_t aria_libc_server_respond_headers(int64_t client_fd, int64_t status_code,
                                          const char *headers, const char *body) {
    int64_t body_len = (int64_t)strlen(body);
    int written = snprintf(g_resp_buf, RESP_BUF_SIZE,
        "HTTP/1.1 %ld %s\r\n"
        "%s"
        "Content-Length: %ld\r\n"
        "Connection: close\r\n"
        "\r\n"
        "%s",
        (long)status_code, status_text(status_code),
        headers, (long)body_len, body);

    if (written <= 0) return 0;
    ssize_t n = send((int)client_fd, g_resp_buf, (size_t)written, 0);
    return (n > 0) ? 1 : 0;
}

/* respond with specific content type */
int64_t aria_libc_server_respond_type(int64_t client_fd, int64_t status_code,
                                       const char *content_type, const char *body) {
    int64_t body_len = (int64_t)strlen(body);
    int written = snprintf(g_resp_buf, RESP_BUF_SIZE,
        "HTTP/1.1 %ld %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %ld\r\n"
        "Connection: close\r\n"
        "\r\n"
        "%s",
        (long)status_code, status_text(status_code),
        content_type, (long)body_len, body);

    if (written <= 0) return 0;
    ssize_t n = send((int)client_fd, g_resp_buf, (size_t)written, 0);
    return (n > 0) ? 1 : 0;
}

/* ════════════════════════════════════════════════════════════════════════════
 * Raw request buffer access (for advanced parsing)
 * ════════════════════════════════════════════════════════════════════════════ */

AriaString aria_libc_server_req_raw(void) {
    return (AriaString){ g_req_buf, g_req_len };
}

int64_t aria_libc_server_req_length(void) {
    return g_req_len;
}
