/* aria_url_shim.c — URL parsing and encoding */
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#define MAX_URL 4096

/* Parsed URL components */
static char url_scheme[64];
static char url_host[256];
static int32_t url_port;
static char url_path[MAX_URL];
static char url_query[MAX_URL];
static char url_fragment[256];

/* Result buffer for encoding/decoding */
static char result_buf[MAX_URL * 3];

/* ---- parse URL ---- */
int32_t aria_url_parse(const char *url) {
    url_scheme[0] = url_host[0] = url_path[0] = url_query[0] = url_fragment[0] = '\0';
    url_port = 0;
    if (!url || !*url) return 0;

    const char *p = url;

    /* scheme */
    const char *colon = strstr(p, "://");
    if (colon) {
        size_t slen = (size_t)(colon - p);
        if (slen >= sizeof(url_scheme)) slen = sizeof(url_scheme) - 1;
        memcpy(url_scheme, p, slen);
        url_scheme[slen] = '\0';
        p = colon + 3;
    }

    /* host and optional port */
    const char *slash = strchr(p, '/');
    const char *qmark = strchr(p, '?');
    const char *hash  = strchr(p, '#');
    const char *end = slash;
    if (!end || (qmark && qmark < end)) end = qmark;
    if (!end || (hash && hash < end)) end = hash;

    size_t hlen;
    if (end) hlen = (size_t)(end - p);
    else     hlen = strlen(p);
    if (hlen >= sizeof(url_host)) hlen = sizeof(url_host) - 1;

    char hostport[256];
    memcpy(hostport, p, hlen);
    hostport[hlen] = '\0';

    /* Check for port */
    char *port_sep = strrchr(hostport, ':');
    if (port_sep && port_sep != hostport) {
        /* Verify it's a number after : */
        int is_port = 1;
        for (const char *c = port_sep + 1; *c; c++) {
            if (!isdigit((unsigned char)*c)) { is_port = 0; break; }
        }
        if (is_port && *(port_sep + 1)) {
            *port_sep = '\0';
            url_port = atoi(port_sep + 1);
        }
    }
    strncpy(url_host, hostport, sizeof(url_host) - 1);
    url_host[sizeof(url_host) - 1] = '\0';
    p += hlen;

    /* path */
    if (p && *p == '/') {
        const char *pend = qmark;
        if (!pend || (hash && hash < pend)) pend = hash;
        size_t plen;
        if (pend) plen = (size_t)(pend - p);
        else      plen = strlen(p);
        if (plen >= sizeof(url_path)) plen = sizeof(url_path) - 1;
        memcpy(url_path, p, plen);
        url_path[plen] = '\0';
        p += plen;
    } else {
        strcpy(url_path, "/");
    }

    /* query */
    if (p && *p == '?') {
        p++;
        const char *qend = hash ? hash : p + strlen(p);
        size_t qlen = (size_t)(qend - p);
        if (qlen >= sizeof(url_query)) qlen = sizeof(url_query) - 1;
        memcpy(url_query, p, qlen);
        url_query[qlen] = '\0';
        p = qend;
    }

    /* fragment */
    if (p && *p == '#') {
        p++;
        strncpy(url_fragment, p, sizeof(url_fragment) - 1);
        url_fragment[sizeof(url_fragment) - 1] = '\0';
    }

    return 1;
}

const char *aria_url_get_scheme(void)   { return url_scheme; }
const char *aria_url_get_host(void)     { return url_host; }
int32_t     aria_url_get_port(void)     { return url_port; }
const char *aria_url_get_path(void)     { return url_path; }
const char *aria_url_get_query(void)    { return url_query; }
const char *aria_url_get_fragment(void) { return url_fragment; }

/* ---- percent-encode ---- */
const char *aria_url_encode(const char *input) {
    char *out = result_buf;
    for (const char *p = input; *p && (out - result_buf) < (int)(sizeof(result_buf) - 4); p++) {
        unsigned char c = (unsigned char)*p;
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            *out++ = (char)c;
        } else {
            snprintf(out, 4, "%%%02X", c);
            out += 3;
        }
    }
    *out = '\0';
    return result_buf;
}

/* ---- percent-decode ---- */
static int hex_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

const char *aria_url_decode(const char *input) {
    char *out = result_buf;
    for (const char *p = input; *p && (out - result_buf) < (int)(sizeof(result_buf) - 2); p++) {
        if (*p == '%' && p[1] && p[2]) {
            int hi = hex_val(p[1]), lo = hex_val(p[2]);
            if (hi >= 0 && lo >= 0) {
                *out++ = (char)((hi << 4) | lo);
                p += 2;
                continue;
            }
        }
        if (*p == '+') *out++ = ' ';
        else *out++ = *p;
    }
    *out = '\0';
    return result_buf;
}
