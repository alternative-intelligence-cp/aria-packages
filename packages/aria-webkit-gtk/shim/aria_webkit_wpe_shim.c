/*  aria_webkit_gtk_shim.c — WebKitGTK bindings for Aria FFI
 *
 *  Wraps WebKitGTK 6.0 for: web view creation, page loading, navigation,
 *  JavaScript execution, title/URI queries, and basic settings.
 *  Uses handle-based pattern with GTK/WebKit under the hood.
 *
 *  Build:
 *    cc -O2 -shared -fPIC -Wall \
 *       $(pkg-config --cflags webkitgtk-6.0) \
 *       -o libaria_webkit_gtk_shim.so aria_webkit_gtk_shim.c \
 *       $(pkg-config --libs webkitgtk-6.0)
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <gtk/gtk.h>
#include <webkit/webkit.h>

/* ── AriaString return ABI ───────────────────────────────────────── */

typedef struct { const char *data; int64_t length; } AriaString;

static char g_str_buf[1024];
static AriaString g_str_ret;

static AriaString make_str(const char *s) {
    size_t len = s ? strlen(s) : 0;
    if (len >= sizeof(g_str_buf)) len = sizeof(g_str_buf) - 1;
    memcpy(g_str_buf, s ? s : "", len);
    g_str_buf[len] = '\0';
    g_str_ret.data = g_str_buf;
    g_str_ret.length = (int64_t)len;
    return g_str_ret;
}

/* ── global state ────────────────────────────────────────────────── */

#define MAX_VIEWS  8

static WebKitWebView *g_views[MAX_VIEWS];
static int            g_wk_initialized = 0;

/* ── init/quit ───────────────────────────────────────────────────── */

int32_t aria_wk_init(void) {
    if (g_wk_initialized) return 0;
    if (!gtk_init_check()) return -1;
    g_wk_initialized = 1;
    return 0;
}

void aria_wk_quit(void) {
    for (int i = 0; i < MAX_VIEWS; i++) {
        if (g_views[i]) { g_views[i] = NULL; }
    }
    g_wk_initialized = 0;
}

int32_t aria_wk_is_init(void) { return g_wk_initialized; }

AriaString aria_wk_get_version(void) {
    static char vbuf[64];
    snprintf(vbuf, sizeof(vbuf), "%u.%u.%u",
             webkit_get_major_version(),
             webkit_get_minor_version(),
             webkit_get_micro_version());
    return make_str(vbuf);
}

/* ── web view creation ───────────────────────────────────────────── */

int32_t aria_wk_create_view(void) {
    if (!g_wk_initialized) return -1;
    for (int32_t i = 0; i < MAX_VIEWS; i++) {
        if (!g_views[i]) {
            g_views[i] = WEBKIT_WEB_VIEW(webkit_web_view_new());
            if (!g_views[i]) return -1;
            /* prevent destruction when unreffed */
            g_object_ref_sink(G_OBJECT(g_views[i]));
            return i;
        }
    }
    return -1;
}

void aria_wk_destroy_view(int32_t h) {
    if (h < 0 || h >= MAX_VIEWS || !g_views[h]) return;
    g_object_unref(G_OBJECT(g_views[h]));
    g_views[h] = NULL;
}

/* ── navigation ──────────────────────────────────────────────────── */

int32_t aria_wk_load_uri(int32_t h, const char *uri) {
    if (h < 0 || h >= MAX_VIEWS || !g_views[h] || !uri) return -1;
    webkit_web_view_load_uri(g_views[h], uri);
    return 0;
}

int32_t aria_wk_load_html(int32_t h, const char *html) {
    if (h < 0 || h >= MAX_VIEWS || !g_views[h] || !html) return -1;
    webkit_web_view_load_html(g_views[h], html, NULL);
    return 0;
}

int32_t aria_wk_go_back(int32_t h) {
    if (h < 0 || h >= MAX_VIEWS || !g_views[h]) return -1;
    webkit_web_view_go_back(g_views[h]);
    return 0;
}

int32_t aria_wk_go_forward(int32_t h) {
    if (h < 0 || h >= MAX_VIEWS || !g_views[h]) return -1;
    webkit_web_view_go_forward(g_views[h]);
    return 0;
}

int32_t aria_wk_reload(int32_t h) {
    if (h < 0 || h >= MAX_VIEWS || !g_views[h]) return -1;
    webkit_web_view_reload(g_views[h]);
    return 0;
}

int32_t aria_wk_stop_loading(int32_t h) {
    if (h < 0 || h >= MAX_VIEWS || !g_views[h]) return -1;
    webkit_web_view_stop_loading(g_views[h]);
    return 0;
}

/* ── queries ─────────────────────────────────────────────────────── */

AriaString aria_wk_get_title(int32_t h) {
    if (h < 0 || h >= MAX_VIEWS || !g_views[h]) return make_str("");
    const char *t = webkit_web_view_get_title(g_views[h]);
    return make_str(t ? t : "");
}

AriaString aria_wk_get_uri(int32_t h) {
    if (h < 0 || h >= MAX_VIEWS || !g_views[h]) return make_str("");
    const char *u = webkit_web_view_get_uri(g_views[h]);
    return make_str(u ? u : "");
}

int32_t aria_wk_is_loading(int32_t h) {
    if (h < 0 || h >= MAX_VIEWS || !g_views[h]) return 0;
    return webkit_web_view_is_loading(g_views[h]) ? 1 : 0;
}

int32_t aria_wk_can_go_back(int32_t h) {
    if (h < 0 || h >= MAX_VIEWS || !g_views[h]) return 0;
    return webkit_web_view_can_go_back(g_views[h]) ? 1 : 0;
}

int32_t aria_wk_can_go_forward(int32_t h) {
    if (h < 0 || h >= MAX_VIEWS || !g_views[h]) return 0;
    return webkit_web_view_can_go_forward(g_views[h]) ? 1 : 0;
}

/* ── settings ────────────────────────────────────────────────────── */

int32_t aria_wk_enable_javascript(int32_t h, int32_t enabled) {
    if (h < 0 || h >= MAX_VIEWS || !g_views[h]) return -1;
    WebKitSettings *settings = webkit_web_view_get_settings(g_views[h]);
    webkit_settings_set_enable_javascript(settings, enabled != 0);
    return 0;
}

int32_t aria_wk_enable_developer_extras(int32_t h, int32_t enabled) {
    if (h < 0 || h >= MAX_VIEWS || !g_views[h]) return -1;
    WebKitSettings *settings = webkit_web_view_get_settings(g_views[h]);
    webkit_settings_set_enable_developer_extras(settings, enabled != 0);
    return 0;
}

/* ================================================================= */
/* ── test harness ────────────────────────────────────────────────── */
/* ================================================================= */

static int t_pass = 0, t_fail = 0;

static void assert_int_eq(int32_t a, int32_t b, const char *msg) {
    if (a == b) { t_pass++; printf("  PASS: %s\n", msg); }
    else        { t_fail++; printf("  FAIL: %s -- got %d, expected %d\n", msg, a, b); }
}

static void assert_str_not_empty(const char *s, const char *msg) {
    if (s && strlen(s) > 0) { t_pass++; printf("  PASS: %s\n", msg); }
    else                    { t_fail++; printf("  FAIL: %s -- got empty/null\n", msg); }
}

int32_t aria_test_webkit_gtk_run(void) {
    printf("=== aria-webkit-gtk tests ===\n");

    /* pre-init */
    assert_int_eq(aria_wk_is_init(), 0, "not initialized at start");

    /* init */
    int32_t r = aria_wk_init();
    assert_int_eq(r, 0, "init returns 0");
    assert_int_eq(aria_wk_is_init(), 1, "is_init after init");
    assert_int_eq(aria_wk_init(), 0, "double init returns 0");

    /* version */
    AriaString ver = aria_wk_get_version();
    assert_str_not_empty(ver.data, "version is non-empty");

    /* create view */
    int32_t v = aria_wk_create_view();
    assert_int_eq(v >= 0 ? 1 : 0, 1, "create_view returns valid handle");

    /* initial state */
    assert_int_eq(aria_wk_is_loading(v), 0, "not loading initially");
    assert_int_eq(aria_wk_can_go_back(v), 0, "cannot go back initially");
    assert_int_eq(aria_wk_can_go_forward(v), 0, "cannot go forward initially");

    /* load HTML */
    assert_int_eq(aria_wk_load_html(v, "<html><body>Hello</body></html>"), 0, "load_html returns 0");

    /* navigation ops (return codes) */
    assert_int_eq(aria_wk_reload(v), 0, "reload returns 0");
    assert_int_eq(aria_wk_stop_loading(v), 0, "stop_loading returns 0");
    assert_int_eq(aria_wk_go_back(v), 0, "go_back returns 0");
    assert_int_eq(aria_wk_go_forward(v), 0, "go_forward returns 0");

    /* settings */
    assert_int_eq(aria_wk_enable_javascript(v, 1), 0, "enable_javascript returns 0");
    assert_int_eq(aria_wk_enable_developer_extras(v, 1), 0, "enable_developer_extras returns 0");

    /* invalid handles */
    assert_int_eq(aria_wk_load_uri(-1, "http://example.com"), -1, "invalid handle load_uri = -1");
    assert_int_eq(aria_wk_is_loading(-1), 0, "invalid handle is_loading = 0");
    assert_int_eq(aria_wk_enable_javascript(-1, 1), -1, "invalid handle enable_js = -1");

    /* cleanup */
    aria_wk_destroy_view(v);
    aria_wk_quit();
    assert_int_eq(aria_wk_is_init(), 0, "not initialized after quit");

    printf("\nResults: %d passed, %d failed (of %d)\n", t_pass, t_fail, t_pass + t_fail);
    return t_fail;
}

#ifdef ARIA_WK_TEST
int main(int argc, char **argv) { return aria_test_webkit_gtk_run(); }
#endif
