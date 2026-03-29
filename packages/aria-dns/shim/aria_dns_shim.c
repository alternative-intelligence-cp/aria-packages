/*  aria_dns_shim.c — DNS resolution for Aria FFI
 *
 *  Wraps getaddrinfo() for hostname→IP resolution.
 *  Supports A (IPv4) and AAAA (IPv6) lookups.
 *  Results stored in static pool, accessed by index.
 *
 *  Build:
 *    cc -O2 -shared -fPIC -Wall -o libaria_dns_shim.so aria_dns_shim.c
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define MAX_DNS_RESULTS   64
#define MAX_ADDR_LEN      64

/* ── result storage ──────────────────────────────────────────────── */

typedef struct {
    char addr[MAX_ADDR_LEN];    /* string IP address */
    int32_t family;             /* 4 = IPv4, 6 = IPv6 */
} DnsRecord;

static DnsRecord g_results[MAX_DNS_RESULTS];
static int32_t   g_result_count = 0;

/* ── resolve ─────────────────────────────────────────────────────── */

/* Resolve hostname. Returns number of results, or -1 on error. */
int32_t aria_dns_resolve(const char *hostname) {
    if (!hostname || !*hostname) return -1;

    g_result_count = 0;
    memset(g_results, 0, sizeof(g_results));

    struct addrinfo hints, *res, *p;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_UNSPEC;     /* IPv4 + IPv6 */
    hints.ai_socktype = SOCK_STREAM;

    int status = getaddrinfo(hostname, NULL, &hints, &res);
    if (status != 0) return -1;

    int32_t count = 0;
    for (p = res; p != NULL && count < MAX_DNS_RESULTS; p = p->ai_next) {
        char buf[MAX_ADDR_LEN];
        if (p->ai_family == AF_INET) {
            struct sockaddr_in *ipv4 = (struct sockaddr_in *)p->ai_addr;
            inet_ntop(AF_INET, &(ipv4->sin_addr), buf, sizeof(buf));
            strncpy(g_results[count].addr, buf, MAX_ADDR_LEN - 1);
            g_results[count].family = 4;
            count++;
        } else if (p->ai_family == AF_INET6) {
            struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)p->ai_addr;
            inet_ntop(AF_INET6, &(ipv6->sin6_addr), buf, sizeof(buf));
            strncpy(g_results[count].addr, buf, MAX_ADDR_LEN - 1);
            g_results[count].family = 6;
            count++;
        }
    }
    freeaddrinfo(res);
    g_result_count = count;
    return count;
}

/* Resolve hostname, IPv4 only. */
int32_t aria_dns_resolve4(const char *hostname) {
    if (!hostname || !*hostname) return -1;

    g_result_count = 0;
    memset(g_results, 0, sizeof(g_results));

    struct addrinfo hints, *res, *p;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    int status = getaddrinfo(hostname, NULL, &hints, &res);
    if (status != 0) return -1;

    int32_t count = 0;
    for (p = res; p != NULL && count < MAX_DNS_RESULTS; p = p->ai_next) {
        if (p->ai_family == AF_INET) {
            struct sockaddr_in *ipv4 = (struct sockaddr_in *)p->ai_addr;
            inet_ntop(AF_INET, &(ipv4->sin_addr),
                      g_results[count].addr, MAX_ADDR_LEN);
            g_results[count].family = 4;
            count++;
        }
    }
    freeaddrinfo(res);
    g_result_count = count;
    return count;
}

/* Resolve hostname, IPv6 only. */
int32_t aria_dns_resolve6(const char *hostname) {
    if (!hostname || !*hostname) return -1;

    g_result_count = 0;
    memset(g_results, 0, sizeof(g_results));

    struct addrinfo hints, *res, *p;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_INET6;
    hints.ai_socktype = SOCK_STREAM;

    int status = getaddrinfo(hostname, NULL, &hints, &res);
    if (status != 0) return -1;

    int32_t count = 0;
    for (p = res; p != NULL && count < MAX_DNS_RESULTS; p = p->ai_next) {
        if (p->ai_family == AF_INET6) {
            struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)p->ai_addr;
            inet_ntop(AF_INET6, &(ipv6->sin6_addr),
                      g_results[count].addr, MAX_ADDR_LEN);
            g_results[count].family = 6;
            count++;
        }
    }
    freeaddrinfo(res);
    g_result_count = count;
    return count;
}

/* ── accessors ───────────────────────────────────────────────────── */

/* Get IP address at index. Returns "" on out-of-range. */
const char *aria_dns_get_addr(int32_t index) {
    if (index < 0 || index >= g_result_count) return "";
    return g_results[index].addr;
}

/* Get address family at index (4 or 6). Returns 0 on invalid. */
int32_t aria_dns_get_family(int32_t index) {
    if (index < 0 || index >= g_result_count) return 0;
    return g_results[index].family;
}

/* Get total result count from last resolve. */
int32_t aria_dns_result_count(void) {
    return g_result_count;
}

/* ── reverse lookup ──────────────────────────────────────────────── */

static char g_hostname_buf[256];

/* Reverse-resolve IP address to hostname. Returns "" on failure. */
const char *aria_dns_reverse(const char *ip_addr) {
    if (!ip_addr || !*ip_addr) return "";

    g_hostname_buf[0] = '\0';

    /* Try IPv4 first, then IPv6 */
    struct sockaddr_in  sa4;
    struct sockaddr_in6 sa6;
    void *sa_ptr = NULL;
    socklen_t sa_len = 0;

    if (inet_pton(AF_INET, ip_addr, &sa4.sin_addr) == 1) {
        sa4.sin_family = AF_INET;
        sa_ptr = &sa4;
        sa_len = sizeof(sa4);
    } else if (inet_pton(AF_INET6, ip_addr, &sa6.sin6_addr) == 1) {
        sa6.sin6_family = AF_INET6;
        sa_ptr = &sa6;
        sa_len = sizeof(sa6);
    } else {
        return "";
    }

    int status = getnameinfo((struct sockaddr *)sa_ptr, sa_len,
                             g_hostname_buf, sizeof(g_hostname_buf),
                             NULL, 0, 0);
    if (status != 0) return "";
    return g_hostname_buf;
}

/* ── test helpers ────────────────────────────────────────────────── */

static int32_t g_test_passed = 0;
static int32_t g_test_failed = 0;

void aria_dns_assert_int_eq(int32_t actual, int32_t expected, const char *msg) {
    if (actual == expected) {
        g_test_passed++;
        printf("[PASS] %s\n", msg);
    } else {
        g_test_failed++;
        printf("[FAIL] %s (expected %d, got %d)\n", msg, expected, actual);
    }
}

void aria_dns_assert_int_gt(int32_t actual, int32_t threshold, const char *msg) {
    if (actual > threshold) {
        g_test_passed++;
        printf("[PASS] %s\n", msg);
    } else {
        g_test_failed++;
        printf("[FAIL] %s (expected >%d, got %d)\n", msg, threshold, actual);
    }
}

void aria_dns_assert_int_ge(int32_t actual, int32_t threshold, const char *msg) {
    if (actual >= threshold) {
        g_test_passed++;
        printf("[PASS] %s\n", msg);
    } else {
        g_test_failed++;
        printf("[FAIL] %s (expected >=%d, got %d)\n", msg, threshold, actual);
    }
}

void aria_dns_assert_str_not_empty(const char *str, const char *msg) {
    if (str && str[0] != '\0') {
        g_test_passed++;
        printf("[PASS] %s (got '%s')\n", msg, str);
    } else {
        g_test_failed++;
        printf("[FAIL] %s (got empty/null)\n", msg);
    }
}

void aria_dns_assert_str_empty(const char *str, const char *msg) {
    if (!str || str[0] == '\0') {
        g_test_passed++;
        printf("[PASS] %s\n", msg);
    } else {
        g_test_failed++;
        printf("[FAIL] %s (expected empty, got '%s')\n", msg, str);
    }
}

void aria_dns_assert_family_valid(int32_t family, const char *msg) {
    if (family == 4 || family == 6) {
        g_test_passed++;
        printf("[PASS] %s (family=%d)\n", msg, family);
    } else {
        g_test_failed++;
        printf("[FAIL] %s (expected 4 or 6, got %d)\n", msg, family);
    }
}

void aria_dns_test_summary(void) {
    printf("\n=== aria-dns test summary ===\n");
    printf("passed=%d failed=%d total=%d\n",
           g_test_passed, g_test_failed, g_test_passed + g_test_failed);
    if (g_test_failed == 0) printf("ALL TESTS PASSED\n");
    else printf("SOME TESTS FAILED\n");
}
