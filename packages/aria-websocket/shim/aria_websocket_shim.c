/*
 * aria_websocket_shim.c — WebSocket client/server shim for Aria
 *
 * Uses POSIX sockets directly to implement the WebSocket protocol (RFC 6455).
 * No external dependencies beyond libc.
 *
 * Architecture:
 *   - Up to 32 concurrent connections (clients or accepted server connections)
 *   - Each connection is a slot with its own socket fd
 *   - WebSocket framing handled internally (text frames, ping/pong, close)
 *   - SHA-1 + Base64 for the opening handshake
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
#include <time.h>

/* ═══════════════════ constants ═══════════════════ */

#define MAX_CONNECTIONS 32
#define RECV_BUF_SIZE   65536
#define SEND_BUF_SIZE   65536
#define MAX_MSG_SIZE    65536

/* WebSocket opcodes */
#define WS_OP_CONTINUATION 0x0
#define WS_OP_TEXT         0x1
#define WS_OP_BINARY       0x2
#define WS_OP_CLOSE        0x8
#define WS_OP_PING         0x9
#define WS_OP_PONG         0xA

/* ═══════════════════ connection state ═══════════════════ */

typedef struct {
    int      fd;
    int      active;
    int      is_server;    /* 1 if this is a server-accepted connection */
    int      handshake_done; /* 0 = upgrade response not yet read (client) */
    char     recv_buf[RECV_BUF_SIZE];
    int      recv_len;
    char     msg_buf[MAX_MSG_SIZE];
    int      msg_len;
    int      msg_ready;
} ws_conn_t;

static ws_conn_t g_conns[MAX_CONNECTIONS];
static int       g_listen_fd = -1;
static char      g_err[512] = "";
static char      g_last_msg[MAX_MSG_SIZE];
static int       g_last_msg_len = 0;

static void set_err(const char *prefix) {
    snprintf(g_err, sizeof(g_err), "%s: %s", prefix, strerror(errno));
}

static int alloc_conn(void) {
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (!g_conns[i].active) return i;
    }
    return -1;
}

/* ═══════════════════ SHA-1 (RFC 3174) ═══════════════════ */

static void sha1_block(uint32_t state[5], const uint8_t block[64]) {
    uint32_t w[80];
    for (int i = 0; i < 16; i++)
        w[i] = ((uint32_t)block[i*4] << 24) | ((uint32_t)block[i*4+1] << 16) |
               ((uint32_t)block[i*4+2] << 8)  | (uint32_t)block[i*4+3];
    for (int i = 16; i < 80; i++) {
        uint32_t t = w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16];
        w[i] = (t << 1) | (t >> 31);
    }
    uint32_t a = state[0], b = state[1], c = state[2], d = state[3], e = state[4];
    for (int i = 0; i < 80; i++) {
        uint32_t f, k;
        if (i < 20)      { f = (b & c) | ((~b) & d); k = 0x5A827999; }
        else if (i < 40) { f = b ^ c ^ d;             k = 0x6ED9EBA1; }
        else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDC; }
        else              { f = b ^ c ^ d;             k = 0xCA62C1D6; }
        uint32_t tmp = ((a << 5) | (a >> 27)) + f + e + k + w[i];
        e = d; d = c; c = (b << 30) | (b >> 2); b = a; a = tmp;
    }
    state[0] += a; state[1] += b; state[2] += c; state[3] += d; state[4] += e;
}

static void sha1(const uint8_t *data, size_t len, uint8_t out[20]) {
    uint32_t state[5] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0};
    size_t i;
    for (i = 0; i + 64 <= len; i += 64)
        sha1_block(state, data + i);
    uint8_t block[64];
    size_t rem = len - i;
    memcpy(block, data + i, rem);
    block[rem++] = 0x80;
    if (rem > 56) {
        memset(block + rem, 0, 64 - rem);
        sha1_block(state, block);
        rem = 0;
    }
    memset(block + rem, 0, 56 - rem);
    uint64_t bits = (uint64_t)len * 8;
    for (int j = 0; j < 8; j++)
        block[56 + j] = (uint8_t)(bits >> (56 - j * 8));
    sha1_block(state, block);
    for (int j = 0; j < 5; j++) {
        out[j*4]   = (uint8_t)(state[j] >> 24);
        out[j*4+1] = (uint8_t)(state[j] >> 16);
        out[j*4+2] = (uint8_t)(state[j] >> 8);
        out[j*4+3] = (uint8_t)(state[j]);
    }
}

/* ═══════════════════ Base64 encode ═══════════════════ */

static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static int base64_encode(const uint8_t *in, size_t len, char *out, size_t out_sz) {
    size_t i, j = 0;
    for (i = 0; i + 2 < len; i += 3) {
        if (j + 4 >= out_sz) return -1;
        out[j++] = b64[in[i] >> 2];
        out[j++] = b64[((in[i] & 3) << 4) | (in[i+1] >> 4)];
        out[j++] = b64[((in[i+1] & 0xF) << 2) | (in[i+2] >> 6)];
        out[j++] = b64[in[i+2] & 0x3F];
    }
    if (i < len) {
        if (j + 4 >= out_sz) return -1;
        out[j++] = b64[in[i] >> 2];
        if (i + 1 < len) {
            out[j++] = b64[((in[i] & 3) << 4) | (in[i+1] >> 4)];
            out[j++] = b64[((in[i+1] & 0xF) << 2)];
        } else {
            out[j++] = b64[((in[i] & 3) << 4)];
            out[j++] = '=';
        }
        out[j++] = '=';
    }
    out[j] = '\0';
    return (int)j;
}

/* ═══════════════════ WebSocket frame helpers ═══════════════════ */

/* Build a WebSocket frame. Returns total frame size, written to `out`.
   mask_flag: 1 if client->server (must mask), 0 if server->client. */
static int ws_build_frame(uint8_t *out, size_t out_sz, int opcode, const uint8_t *payload, size_t plen, int mask_flag) {
    size_t hdr = 2;
    out[0] = 0x80 | (opcode & 0x0F); /* FIN + opcode */

    if (plen < 126) {
        out[1] = (uint8_t)plen;
    } else if (plen <= 65535) {
        out[1] = 126;
        out[2] = (uint8_t)(plen >> 8);
        out[3] = (uint8_t)(plen);
        hdr = 4;
    } else {
        out[1] = 127;
        for (int i = 0; i < 8; i++)
            out[2 + i] = (uint8_t)(plen >> (56 - i * 8));
        hdr = 10;
    }

    if (mask_flag) {
        out[1] |= 0x80;
        uint8_t mask[4];
        uint32_t r = (uint32_t)rand();
        memcpy(mask, &r, 4);
        memcpy(out + hdr, mask, 4);
        hdr += 4;
        if (hdr + plen > out_sz) return -1;
        for (size_t i = 0; i < plen; i++)
            out[hdr + i] = payload[i] ^ mask[i % 4];
    } else {
        if (hdr + plen > out_sz) return -1;
        memcpy(out + hdr, payload, plen);
    }
    return (int)(hdr + plen);
}

/* Parse a WebSocket frame from `data` of `len` bytes.
   Returns bytes consumed (0 if incomplete), sets *opcode, *payload, *plen.
   Handles unmasking. */
static int ws_parse_frame(const uint8_t *data, size_t len, int *opcode, uint8_t *payload, size_t *plen) {
    if (len < 2) return 0;
    *opcode = data[0] & 0x0F;
    int masked = (data[1] & 0x80) != 0;
    uint64_t payload_len = data[1] & 0x7F;
    size_t hdr = 2;

    if (payload_len == 126) {
        if (len < 4) return 0;
        payload_len = ((uint64_t)data[2] << 8) | data[3];
        hdr = 4;
    } else if (payload_len == 127) {
        if (len < 10) return 0;
        payload_len = 0;
        for (int i = 0; i < 8; i++)
            payload_len = (payload_len << 8) | data[2 + i];
        hdr = 10;
    }

    if (masked) hdr += 4;
    if (len < hdr + payload_len) return 0;
    if (payload_len > MAX_MSG_SIZE) {
        /* Message too large — skip it */
        return (int)(hdr + payload_len);
    }

    if (masked) {
        const uint8_t *mask = data + hdr - 4;
        for (uint64_t i = 0; i < payload_len; i++)
            payload[i] = data[hdr + i] ^ mask[i % 4];
    } else {
        memcpy(payload, data + hdr, payload_len);
    }
    *plen = (size_t)payload_len;
    return (int)(hdr + payload_len);
}

/* ═══════════════════ public API ═══════════════════ */

const char *aria_ws_error(void) { return g_err; }

const char *aria_ws_last_message(void) { return g_last_msg; }

int32_t aria_ws_last_message_len(void) { return (int32_t)g_last_msg_len; }

/* ── client connect ────────────────────────────────── */

int32_t aria_ws_connect(const char *host, int32_t port, const char *path) {
    g_err[0] = '\0';

    int slot = alloc_conn();
    if (slot < 0) {
        snprintf(g_err, sizeof(g_err), "ws_connect: no free slots");
        return -1;
    }

    /* TCP connect */
    struct addrinfo hints, *res, *p;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", port);

    int rv = getaddrinfo(host, port_str, &hints, &res);
    if (rv != 0) {
        snprintf(g_err, sizeof(g_err), "ws_connect: %s", gai_strerror(rv));
        return -1;
    }

    int fd = -1;
    for (p = res; p; p = p->ai_next) {
        fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (fd < 0) continue;
        if (connect(fd, p->ai_addr, p->ai_addrlen) == 0) break;
        close(fd);
        fd = -1;
    }
    freeaddrinfo(res);
    if (fd < 0) { set_err("ws_connect"); return -1; }

    /* Generate 16-byte random key, base64 encode */
    srand((unsigned)time(NULL) ^ (unsigned)fd);
    uint8_t key_raw[16];
    for (int i = 0; i < 16; i++) key_raw[i] = (uint8_t)(rand() & 0xFF);
    char key_b64[32];
    base64_encode(key_raw, 16, key_b64, sizeof(key_b64));

    /* Send HTTP upgrade request */
    char req[1024];
    int rlen = snprintf(req, sizeof(req),
        "GET %s HTTP/1.1\r\n"
        "Host: %s:%d\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: %s\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "\r\n",
        path, host, port, key_b64);

    if (send(fd, req, rlen, 0) != rlen) {
        close(fd);
        set_err("ws_connect(send upgrade)");
        return -1;
    }

    /* Try to read HTTP response (non-blocking — server may not have accepted yet) */
    ws_conn_t *c = &g_conns[slot];
    memset(c, 0, sizeof(*c));
    c->fd = fd;
    c->active = 1;
    c->is_server = 0;
    c->handshake_done = 0;

    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = POLLIN;
    int pr = poll(&pfd, 1, 200); /* 200ms timeout */
    if (pr > 0) {
        char resp[2048];
        int rr = recv(fd, resp, sizeof(resp) - 1, 0);
        if (rr > 0) {
            resp[rr] = '\0';
            if (strstr(resp, "101") != NULL) {
                c->handshake_done = 1;
            }
        }
    }
    /* If handshake not done yet, it will complete on first send/recv */
    return (int32_t)slot;
}

/* ── server listen ─────────────────────────────────── */

int32_t aria_ws_listen(const char *bind_addr, int32_t port, int32_t backlog) {
    g_err[0] = '\0';

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { set_err("ws_listen(socket)"); return -1; }

    int yes = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port   = htons((uint16_t)port);
    if (bind_addr[0] == '0')
        addr.sin_addr.s_addr = INADDR_ANY;
    else
        inet_pton(AF_INET, bind_addr, &addr.sin_addr);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        set_err("ws_listen(bind)");
        return -1;
    }
    if (listen(fd, backlog) < 0) {
        close(fd);
        set_err("ws_listen(listen)");
        return -1;
    }

    g_listen_fd = fd;
    return (int32_t)fd;
}

/* ── server accept ─────────────────────────────────── */

int32_t aria_ws_accept(int32_t listen_fd, int32_t timeout_ms) {
    g_err[0] = '\0';

    /* Poll for incoming connection */
    struct pollfd pfd;
    pfd.fd = listen_fd;
    pfd.events = POLLIN;
    int pr = poll(&pfd, 1, timeout_ms);
    if (pr == 0) return -2; /* timeout */
    if (pr < 0) { set_err("ws_accept(poll)"); return -1; }

    int client_fd = accept(listen_fd, NULL, NULL);
    if (client_fd < 0) { set_err("ws_accept"); return -1; }

    /* Read HTTP upgrade request */
    char req[4096];
    int rr = recv(client_fd, req, sizeof(req) - 1, 0);
    if (rr <= 0) {
        close(client_fd);
        set_err("ws_accept(recv)");
        return -1;
    }
    req[rr] = '\0';

    /* Extract Sec-WebSocket-Key */
    char *key_start = strstr(req, "Sec-WebSocket-Key: ");
    if (!key_start) {
        close(client_fd);
        snprintf(g_err, sizeof(g_err), "ws_accept: no WebSocket key in request");
        return -1;
    }
    key_start += 19; /* strlen("Sec-WebSocket-Key: ") */
    char *key_end = strstr(key_start, "\r\n");
    if (!key_end) {
        close(client_fd);
        snprintf(g_err, sizeof(g_err), "ws_accept: malformed key header");
        return -1;
    }

    char client_key[128];
    size_t klen = (size_t)(key_end - key_start);
    if (klen >= sizeof(client_key)) klen = sizeof(client_key) - 1;
    memcpy(client_key, key_start, klen);
    client_key[klen] = '\0';

    /* Compute accept key: SHA1(client_key + magic GUID) → base64 */
    static const char *ws_guid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    char concat[256];
    snprintf(concat, sizeof(concat), "%s%s", client_key, ws_guid);
    uint8_t hash[20];
    sha1((const uint8_t *)concat, strlen(concat), hash);
    char accept_b64[64];
    base64_encode(hash, 20, accept_b64, sizeof(accept_b64));

    /* Send upgrade response */
    char resp[512];
    int wlen = snprintf(resp, sizeof(resp),
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: %s\r\n"
        "\r\n",
        accept_b64);

    if (send(client_fd, resp, wlen, 0) != wlen) {
        close(client_fd);
        set_err("ws_accept(send response)");
        return -1;
    }

    int slot = alloc_conn();
    if (slot < 0) {
        close(client_fd);
        snprintf(g_err, sizeof(g_err), "ws_accept: no free slots");
        return -1;
    }

    ws_conn_t *c = &g_conns[slot];
    memset(c, 0, sizeof(*c));
    c->fd = client_fd;
    c->active = 1;
    c->is_server = 1;
    c->handshake_done = 1;
    return (int32_t)slot;
}

/* ── send text message ─────────────────────────────── */

/* Complete deferred client handshake if needed */
static int ws_complete_handshake(ws_conn_t *c, int timeout_ms) {
    if (c->handshake_done || c->is_server) return 0;
    struct pollfd pfd;
    pfd.fd = c->fd;
    pfd.events = POLLIN;
    int pr = poll(&pfd, 1, timeout_ms);
    if (pr <= 0) return -1;
    char resp[2048];
    int rr = recv(c->fd, resp, sizeof(resp) - 1, 0);
    if (rr <= 0) return -1;
    resp[rr] = '\0';
    if (strstr(resp, "101") == NULL) return -1;
    c->handshake_done = 1;
    return 0;
}

int32_t aria_ws_send(int32_t conn_id, const char *msg) {
    g_err[0] = '\0';
    if (conn_id < 0 || conn_id >= MAX_CONNECTIONS || !g_conns[conn_id].active) {
        snprintf(g_err, sizeof(g_err), "ws_send: invalid connection %d", conn_id);
        return -1;
    }

    ws_conn_t *c = &g_conns[conn_id];

    /* Complete deferred handshake if needed */
    if (!c->handshake_done && !c->is_server) {
        if (ws_complete_handshake(c, 2000) < 0) {
            snprintf(g_err, sizeof(g_err), "ws_send: handshake incomplete");
            return -1;
        }
    }
    size_t mlen = strlen(msg);
    int mask_flag = c->is_server ? 0 : 1; /* clients mask, servers don't */

    uint8_t frame[SEND_BUF_SIZE];
    int flen = ws_build_frame(frame, sizeof(frame), WS_OP_TEXT, (const uint8_t *)msg, mlen, mask_flag);
    if (flen < 0) {
        snprintf(g_err, sizeof(g_err), "ws_send: message too large");
        return -1;
    }

    ssize_t sent = send(c->fd, frame, flen, 0);
    if (sent != flen) {
        set_err("ws_send");
        return -1;
    }
    return 0;
}

/* ── receive message (poll + read) ─────────────────── */

int32_t aria_ws_recv(int32_t conn_id, int32_t timeout_ms) {
    g_err[0] = '\0';
    if (conn_id < 0 || conn_id >= MAX_CONNECTIONS || !g_conns[conn_id].active) {
        snprintf(g_err, sizeof(g_err), "ws_recv: invalid connection %d", conn_id);
        return -1;
    }

    ws_conn_t *c = &g_conns[conn_id];

    /* Complete deferred handshake if needed */
    if (!c->handshake_done && !c->is_server) {
        if (ws_complete_handshake(c, 2000) < 0) {
            snprintf(g_err, sizeof(g_err), "ws_recv: handshake incomplete");
            return -1;
        }
    }

    /* Poll for data */
    struct pollfd pfd;
    pfd.fd = c->fd;
    pfd.events = POLLIN;
    int pr = poll(&pfd, 1, timeout_ms);
    if (pr == 0) return -2; /* timeout — no data */
    if (pr < 0) { set_err("ws_recv(poll)"); return -1; }

    /* Read available data into buffer */
    int space = RECV_BUF_SIZE - c->recv_len;
    if (space <= 0) {
        snprintf(g_err, sizeof(g_err), "ws_recv: buffer full");
        return -1;
    }
    ssize_t n = recv(c->fd, c->recv_buf + c->recv_len, space, 0);
    if (n <= 0) {
        if (n == 0)
            snprintf(g_err, sizeof(g_err), "ws_recv: connection closed");
        else
            set_err("ws_recv");
        c->active = 0;
        close(c->fd);
        return -1;
    }
    c->recv_len += (int)n;

    /* Try to parse a frame */
    int opcode = 0;
    size_t plen = 0;
    uint8_t payload[MAX_MSG_SIZE];
    int consumed = ws_parse_frame((const uint8_t *)c->recv_buf, c->recv_len, &opcode, payload, &plen);
    if (consumed == 0) return -2; /* incomplete frame, wait for more */

    /* Shift remaining data */
    c->recv_len -= consumed;
    if (c->recv_len > 0)
        memmove(c->recv_buf, c->recv_buf + consumed, c->recv_len);

    /* Handle control frames */
    if (opcode == WS_OP_PING) {
        /* Send pong */
        uint8_t pong[SEND_BUF_SIZE];
        int mask_flag = c->is_server ? 0 : 1;
        int flen = ws_build_frame(pong, sizeof(pong), WS_OP_PONG, payload, plen, mask_flag);
        if (flen > 0) send(c->fd, pong, flen, 0);
        return -2; /* no user message */
    }
    if (opcode == WS_OP_CLOSE) {
        c->active = 0;
        close(c->fd);
        snprintf(g_err, sizeof(g_err), "ws_recv: close frame received");
        return -3; /* closed */
    }
    if (opcode == WS_OP_PONG) {
        return -2; /* ignore pong, not a user message */
    }

    /* Text or binary message */
    if (plen >= MAX_MSG_SIZE) plen = MAX_MSG_SIZE - 1;
    memcpy(g_last_msg, payload, plen);
    g_last_msg[plen] = '\0';
    g_last_msg_len = (int)plen;
    return 0;
}

/* ── close connection ──────────────────────────────── */

void aria_ws_close(int32_t conn_id) {
    if (conn_id < 0 || conn_id >= MAX_CONNECTIONS) return;
    ws_conn_t *c = &g_conns[conn_id];
    if (!c->active) return;

    /* Send close frame */
    uint8_t frame[16];
    int mask_flag = c->is_server ? 0 : 1;
    int flen = ws_build_frame(frame, sizeof(frame), WS_OP_CLOSE, NULL, 0, mask_flag);
    if (flen > 0) send(c->fd, frame, flen, 0);

    close(c->fd);
    c->active = 0;
    c->fd = -1;
}

/* ── close listen socket ───────────────────────────── */

void aria_ws_close_listener(int32_t listen_fd) {
    if (listen_fd >= 0) close(listen_fd);
    if (g_listen_fd == listen_fd) g_listen_fd = -1;
}

/* ── broadcast (send to all active connections) ────── */

int32_t aria_ws_broadcast(const char *msg) {
    g_err[0] = '\0';
    int count = 0;
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (g_conns[i].active) {
            if (aria_ws_send(i, msg) == 0) count++;
        }
    }
    return count;
}

/* ── connection count ──────────────────────────────── */

int32_t aria_ws_connection_count(void) {
    int count = 0;
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (g_conns[i].active) count++;
    }
    return count;
}

/* ── ping ──────────────────────────────────────────── */

int32_t aria_ws_ping(int32_t conn_id) {
    g_err[0] = '\0';
    if (conn_id < 0 || conn_id >= MAX_CONNECTIONS || !g_conns[conn_id].active) {
        snprintf(g_err, sizeof(g_err), "ws_ping: invalid connection %d", conn_id);
        return -1;
    }
    ws_conn_t *c = &g_conns[conn_id];
    int mask_flag = c->is_server ? 0 : 1;
    uint8_t frame[16];
    int flen = ws_build_frame(frame, sizeof(frame), WS_OP_PING, NULL, 0, mask_flag);
    if (flen < 0) return -1;
    ssize_t sent = send(c->fd, frame, flen, 0);
    return sent > 0 ? 0 : -1;
}
