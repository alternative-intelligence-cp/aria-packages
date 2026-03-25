/*
 * aria_server_shim.c — HTTP/1.1 server shim for Aria
 *
 * Provides: listen, accept, request parsing, response sending.
 * Single-threaded, synchronous per-connection. The Aria program
 * drives the accept loop and decides how to handle each request.
 *
 * Build:
 *   gcc -shared -fPIC -o libaria_server_shim.so aria_server_shim.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>

/* ------------------------------------------------------------------ */
/* Constants                                                           */
/* ------------------------------------------------------------------ */

#define MAX_REQUEST_SIZE   (1024 * 64)   /* 64 KB max request         */
#define MAX_HEADERS        64            /* max headers per request    */
#define MAX_RESPONSE_SIZE  (1024 * 256)  /* 256 KB response buffer    */

/* ------------------------------------------------------------------ */
/* AriaString compatibility                                            */
/* ------------------------------------------------------------------ */

typedef struct {
    char*    data;
    int64_t  length;
} AriaString;

/* Create a heap-allocated AriaString* from a C string.
 * The data pointer points into the original C string (no copy).
 * This matches what the Aria codegen expects for string returns. */
static AriaString* make_aria_string(const char* s) {
    AriaString* as = (AriaString*)malloc(sizeof(AriaString));
    as->data   = (char*)s;
    as->length = s ? (int64_t)strlen(s) : 0;
    return as;
}

/* ------------------------------------------------------------------ */
/* Global state                                                        */
/* ------------------------------------------------------------------ */

static char g_err[512]  = "";
static int  g_listen_fd = -1;

/* Parsed request fields — valid after aria_server_accept() */
static char g_method[16]              = "";
static char g_path[2048]              = "";
static char g_query[2048]             = "";
static char g_version[16]             = "";
static char g_body[MAX_REQUEST_SIZE]  = "";
static int  g_body_len                = 0;
static int  g_client_fd               = -1;

/* Header storage */
static char g_hdr_names[MAX_HEADERS][256];
static char g_hdr_values[MAX_HEADERS][2048];
static int  g_hdr_count = 0;

/* Response buffer */
static char g_resp[MAX_RESPONSE_SIZE];

/* Peer info */
static char g_peer_addr[INET6_ADDRSTRLEN] = "";
static int  g_peer_port = 0;

/* Test helpers */
static int g_test_pass = 0;
static int g_test_fail = 0;

/* ------------------------------------------------------------------ */
/* Internal helpers                                                    */
/* ------------------------------------------------------------------ */

static void set_err(const char* msg) {
    snprintf(g_err, sizeof(g_err), "%s", msg);
}

static void set_err_errno(const char* prefix) {
    snprintf(g_err, sizeof(g_err), "%s: %s", prefix, strerror(errno));
}

/* Case-insensitive header name compare */
static int hdr_name_eq(const char* a, const char* b) {
    while (*a && *b) {
        char ca = *a, cb = *b;
        if (ca >= 'A' && ca <= 'Z') ca += 32;
        if (cb >= 'A' && cb <= 'Z') cb += 32;
        if (ca != cb) return 0;
        a++; b++;
    }
    return *a == *b;
}

/* Parse the request line and headers from raw data.
 * Returns pointer to body start, or NULL on parse error.        */
static const char* parse_request(const char* raw, int raw_len) {
    g_method[0] = g_path[0] = g_query[0] = g_version[0] = g_body[0] = 0;
    g_body_len  = 0;
    g_hdr_count = 0;

    /* Find end of request line */
    const char* line_end = strstr(raw, "\r\n");
    if (!line_end) { set_err("malformed request: no CRLF"); return NULL; }

    /* Parse: METHOD SP PATH[?QUERY] SP HTTP/x.y */
    int line_len = (int)(line_end - raw);
    char line[4096];
    if (line_len >= (int)sizeof(line)) line_len = (int)sizeof(line) - 1;
    memcpy(line, raw, line_len);
    line[line_len] = 0;

    /* Method */
    char* sp = strchr(line, ' ');
    if (!sp) { set_err("malformed request line"); return NULL; }
    *sp = 0;
    snprintf(g_method, sizeof(g_method), "%s", line);

    /* URI (path + optional query) */
    char* uri = sp + 1;
    sp = strchr(uri, ' ');
    if (!sp) { set_err("malformed request line: no version"); return NULL; }
    *sp = 0;
    snprintf(g_version, sizeof(g_version), "%s", sp + 1);

    /* Split URI into path and query */
    char* qmark = strchr(uri, '?');
    if (qmark) {
        *qmark = 0;
        snprintf(g_path, sizeof(g_path), "%s", uri);
        snprintf(g_query, sizeof(g_query), "%s", qmark + 1);
    } else {
        snprintf(g_path, sizeof(g_path), "%s", uri);
        g_query[0] = 0;
    }

    /* Parse headers */
    const char* p = line_end + 2; /* skip first \r\n */
    while (p < raw + raw_len) {
        if (p[0] == '\r' && p[1] == '\n') {
            /* Empty line = end of headers */
            return p + 2;
        }
        const char* hdr_end = strstr(p, "\r\n");
        if (!hdr_end) break;

        /* Find colon */
        const char* colon = memchr(p, ':', hdr_end - p);
        if (colon && g_hdr_count < MAX_HEADERS) {
            int nlen = (int)(colon - p);
            if (nlen > 255) nlen = 255;
            memcpy(g_hdr_names[g_hdr_count], p, nlen);
            g_hdr_names[g_hdr_count][nlen] = 0;

            /* Skip ": " */
            const char* val = colon + 1;
            while (val < hdr_end && *val == ' ') val++;
            int vlen = (int)(hdr_end - val);
            if (vlen > 2047) vlen = 2047;
            memcpy(g_hdr_values[g_hdr_count], val, vlen);
            g_hdr_values[g_hdr_count][vlen] = 0;

            g_hdr_count++;
        }
        p = hdr_end + 2;
    }
    return NULL; /* headers never terminated properly */
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

/* Return last error string */
const char* aria_server_error(void) {
    return g_err;
}

/* Start listening on addr:port. Returns listen fd or -1.            */
int32_t aria_server_listen(const char* addr, int32_t port, int32_t backlog) {
    /* Ignore SIGPIPE so writes to closed connections don't crash */
    signal(SIGPIPE, SIG_IGN);

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { set_err_errno("socket"); return -1; }

    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family      = AF_INET;
    sa.sin_port        = htons((uint16_t)port);
    sa.sin_addr.s_addr = inet_addr(addr);

    if (bind(fd, (struct sockaddr*)&sa, sizeof(sa)) < 0) {
        set_err_errno("bind");
        close(fd);
        return -1;
    }
    if (listen(fd, backlog) < 0) {
        set_err_errno("listen");
        close(fd);
        return -1;
    }

    g_listen_fd = fd;
    g_err[0] = 0;
    return fd;
}

/* Accept one connection, read and parse the HTTP request.
 * Blocks until a client connects (or timeout_ms expires; -1 = forever).
 * Returns client fd (>= 0) on success, -1 on error/timeout.
 * After success, use aria_server_req_* to access parsed fields.     */
int32_t aria_server_accept(int32_t listen_fd, int32_t timeout_ms) {
    /* Poll for incoming connection */
    struct pollfd pfd = { .fd = listen_fd, .events = POLLIN };
    int pr = poll(&pfd, 1, timeout_ms);
    if (pr == 0) { set_err("accept timeout"); return -1; }
    if (pr < 0) { set_err_errno("poll"); return -1; }

    struct sockaddr_in ca;
    socklen_t ca_len = sizeof(ca);
    int cfd = accept(listen_fd, (struct sockaddr*)&ca, &ca_len);
    if (cfd < 0) { set_err_errno("accept"); return -1; }

    /* Record peer info */
    inet_ntop(AF_INET, &ca.sin_addr, g_peer_addr, sizeof(g_peer_addr));
    g_peer_port = ntohs(ca.sin_port);

    /* Set read timeout to avoid hanging on slow clients */
    struct timeval tv = { .tv_sec = 5, .tv_usec = 0 };
    setsockopt(cfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    /* Read raw request */
    char raw[MAX_REQUEST_SIZE];
    int total = 0;
    int got_headers = 0;
    const char* body_start = NULL;
    int content_length = 0;

    while (total < MAX_REQUEST_SIZE - 1) {
        int n = recv(cfd, raw + total, MAX_REQUEST_SIZE - 1 - total, 0);
        if (n <= 0) break;
        total += n;
        raw[total] = 0;

        /* Check if we have the full headers */
        if (!got_headers) {
            char* hdr_end = strstr(raw, "\r\n\r\n");
            if (hdr_end) {
                got_headers = 1;
                body_start = hdr_end + 4;

                /* Find Content-Length header */
                /* Simple scan — handles "Content-Length: NNN\r\n" */
                const char* cl = raw;
                while (cl < hdr_end) {
                    if ((cl[0] == 'C' || cl[0] == 'c') &&
                        strncasecmp(cl, "Content-Length:", 15) == 0) {
                        content_length = atoi(cl + 15);
                        break;
                    }
                    const char* nl = strstr(cl, "\r\n");
                    if (!nl) break;
                    cl = nl + 2;
                }

                /* If no body expected, we're done */
                if (content_length <= 0) break;

                /* Check if we already have the full body */
                int body_received = total - (int)(body_start - raw);
                if (body_received >= content_length) break;
            }
        } else {
            /* We have headers, check if body is complete */
            int body_received = total - (int)(body_start - raw);
            if (body_received >= content_length) break;
        }
    }

    /* Parse the request */
    const char* parsed_body = parse_request(raw, total);
    if (!parsed_body && g_err[0]) {
        close(cfd);
        return -1;
    }

    /* Copy body */
    if (parsed_body && content_length > 0) {
        int body_avail = total - (int)(parsed_body - raw);
        if (body_avail > content_length) body_avail = content_length;
        if (body_avail > MAX_REQUEST_SIZE - 1) body_avail = MAX_REQUEST_SIZE - 1;
        memcpy(g_body, parsed_body, body_avail);
        g_body[body_avail] = 0;
        g_body_len = body_avail;
    }

    g_client_fd = cfd;
    g_err[0] = 0;
    return cfd;
}

/* ------------------------------------------------------------------ */
/* Request accessors (valid after successful accept)                   */
/* ------------------------------------------------------------------ */

const char* aria_server_req_method(void) { return g_method; }
const char* aria_server_req_path(void)   { return g_path; }
const char* aria_server_req_query(void)  { return g_query; }
const char* aria_server_req_version(void){ return g_version; }
const char* aria_server_req_body(void)   { return g_body; }
int32_t     aria_server_req_body_length(void) { return g_body_len; }

/* Get header by name (case-insensitive). Returns "" if not found.   */
const char* aria_server_req_header(const char* name) {
    for (int i = 0; i < g_hdr_count; i++) {
        if (hdr_name_eq(g_hdr_names[i], name))
            return g_hdr_values[i];
    }
    return "";
}

/* Number of headers */
int32_t aria_server_req_header_count(void) { return g_hdr_count; }

/* Get header name/value by index */
const char* aria_server_req_header_name(int32_t i) {
    if (i < 0 || i >= g_hdr_count) return "";
    return g_hdr_names[i];
}
const char* aria_server_req_header_value(int32_t i) {
    if (i < 0 || i >= g_hdr_count) return "";
    return g_hdr_values[i];
}

/* Peer info */
const char* aria_server_peer_addr(void)  { return g_peer_addr; }
int32_t     aria_server_peer_port(void)  { return g_peer_port; }

/* ------------------------------------------------------------------ */
/* Response sending                                                    */
/* ------------------------------------------------------------------ */

/* Send a complete HTTP response. Returns bytes sent or -1.          */
int64_t aria_server_respond(int32_t client_fd,
                            int32_t status_code,
                            const char* status_text,
                            const char* content_type,
                            const char* body)
{
    int body_len = body ? (int)strlen(body) : 0;
    int n = snprintf(g_resp, sizeof(g_resp),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "\r\n",
        status_code, status_text,
        content_type,
        body_len);

    if (n < 0 || n >= (int)sizeof(g_resp)) {
        set_err("response too large for buffer");
        return -1;
    }

    /* Append body */
    if (body_len > 0 && n + body_len < (int)sizeof(g_resp)) {
        memcpy(g_resp + n, body, body_len);
        n += body_len;
    }

    /* Send all */
    int total_sent = 0;
    while (total_sent < n) {
        int s = send(client_fd, g_resp + total_sent, n - total_sent, 0);
        if (s <= 0) {
            set_err_errno("send");
            return -1;
        }
        total_sent += s;
    }

    g_err[0] = 0;
    return total_sent;
}

/* Send response with extra headers (newline-separated "Key: Value\n") */
int64_t aria_server_respond_headers(int32_t client_fd,
                                     int32_t status_code,
                                     const char* status_text,
                                     const char* content_type,
                                     const char* extra_headers,
                                     const char* body)
{
    int body_len = body ? (int)strlen(body) : 0;

    /* Build status line + standard headers */
    int n = snprintf(g_resp, sizeof(g_resp),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %d\r\n",
        status_code, status_text,
        content_type,
        body_len);

    /* Add extra headers — convert \n to \r\n */
    if (extra_headers && extra_headers[0]) {
        const char* p = extra_headers;
        while (*p && n < (int)sizeof(g_resp) - 4) {
            if (*p == '\n') {
                g_resp[n++] = '\r';
                g_resp[n++] = '\n';
                p++;
            } else {
                g_resp[n++] = *p++;
            }
        }
        /* Ensure trailing CRLF for last header if not present */
        if (n >= 2 && !(g_resp[n-2] == '\r' && g_resp[n-1] == '\n')) {
            g_resp[n++] = '\r';
            g_resp[n++] = '\n';
        }
    }

    /* Connection: close + empty line */
    n += snprintf(g_resp + n, sizeof(g_resp) - n,
        "Connection: close\r\n\r\n");

    /* Append body */
    if (body_len > 0 && n + body_len < (int)sizeof(g_resp)) {
        memcpy(g_resp + n, body, body_len);
        n += body_len;
    }

    /* Send all */
    int total_sent = 0;
    while (total_sent < n) {
        int s = send(client_fd, g_resp + total_sent, n - total_sent, 0);
        if (s <= 0) {
            set_err_errno("send");
            return -1;
        }
        total_sent += s;
    }

    g_err[0] = 0;
    return total_sent;
}

/* Close a client connection */
void aria_server_close_client(int32_t client_fd) {
    if (client_fd >= 0) close(client_fd);
}

/* Stop the listener */
void aria_server_close(int32_t listen_fd) {
    if (listen_fd >= 0) close(listen_fd);
    if (listen_fd == g_listen_fd) g_listen_fd = -1;
}

/* ------------------------------------------------------------------ */
/* Test helpers                                                        */
/* ------------------------------------------------------------------ */

void aria_server_assert_eq(const char* label, const char* expected, const char* actual) {
    if (strcmp(expected, actual) == 0) {
        g_test_pass++;
        fprintf(stderr, "  PASS: %s\n", label);
    } else {
        g_test_fail++;
        fprintf(stderr, "  FAIL: %s — expected \"%s\", got \"%s\"\n",
                label, expected, actual);
    }
}

void aria_server_assert_int_eq(const char* label, int32_t expected, int32_t actual) {
    if (expected == actual) {
        g_test_pass++;
        fprintf(stderr, "  PASS: %s\n", label);
    } else {
        g_test_fail++;
        fprintf(stderr, "  FAIL: %s — expected %d, got %d\n",
                label, expected, actual);
    }
}

void aria_server_test_summary(void) {
    fprintf(stderr, "\n--- aria-server: %d passed, %d failed ---\n",
            g_test_pass, g_test_fail);
}
