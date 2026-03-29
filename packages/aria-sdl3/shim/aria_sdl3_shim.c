/*  aria_sdl3_shim.c — SDL3 bindings for Aria FFI
 *
 *  Wraps SDL3 for: initialization, window creation, 2D rendering,
 *  event polling, keyboard/mouse state, and basic drawing.
 *  Uses handle-based pattern for windows and renderers.
 *
 *  Build:
 *    cc -O2 -shared -fPIC -Wall -o libaria_sdl3_shim.so aria_sdl3_shim.c \
 *       -I../SDL-release-3.4.2/include -L../SDL-release-3.4.2/build -lSDL3
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <SDL3/SDL.h>

/* ── AriaString return ABI ───────────────────────────────────────── */

typedef struct { const char *data; int64_t length; } AriaString;

static char g_str_buf[512];
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

#define MAX_WINDOWS     8
#define MAX_RENDERERS   8

static SDL_Window   *g_windows[MAX_WINDOWS];
static SDL_Renderer *g_renderers[MAX_RENDERERS];
static SDL_Event     g_last_event;
static int           g_initialized = 0;

/* ── init/quit ───────────────────────────────────────────────────── */

int32_t aria_sdl3_init(void) {
    if (g_initialized) return 0;
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        return -1;
    }
    g_initialized = 1;
    return 0;
}

void aria_sdl3_quit(void) {
    for (int i = 0; i < MAX_RENDERERS; i++) {
        if (g_renderers[i]) { SDL_DestroyRenderer(g_renderers[i]); g_renderers[i] = NULL; }
    }
    for (int i = 0; i < MAX_WINDOWS; i++) {
        if (g_windows[i]) { SDL_DestroyWindow(g_windows[i]); g_windows[i] = NULL; }
    }
    if (g_initialized) { SDL_Quit(); g_initialized = 0; }
}

int32_t aria_sdl3_is_init(void) {
    return g_initialized ? 1 : 0;
}

AriaString aria_sdl3_get_error(void) {
    return make_str(SDL_GetError());
}

/* ── window ──────────────────────────────────────────────────────── */

int32_t aria_sdl3_create_window(const char *title, int32_t w, int32_t h) {
    if (!g_initialized) return -1;
    for (int32_t i = 0; i < MAX_WINDOWS; i++) {
        if (!g_windows[i]) {
            g_windows[i] = SDL_CreateWindow(title ? title : "Aria SDL3", w, h, 0);
            if (!g_windows[i]) return -1;
            return i;
        }
    }
    return -1;
}

void aria_sdl3_destroy_window(int32_t h) {
    if (h < 0 || h >= MAX_WINDOWS || !g_windows[h]) return;
    SDL_DestroyWindow(g_windows[h]);
    g_windows[h] = NULL;
}

int32_t aria_sdl3_get_window_width(int32_t h) {
    if (h < 0 || h >= MAX_WINDOWS || !g_windows[h]) return 0;
    int w = 0, ht = 0;
    SDL_GetWindowSize(g_windows[h], &w, &ht);
    return (int32_t)w;
}

int32_t aria_sdl3_get_window_height(int32_t h) {
    if (h < 0 || h >= MAX_WINDOWS || !g_windows[h]) return 0;
    int w = 0, ht = 0;
    SDL_GetWindowSize(g_windows[h], &w, &ht);
    return (int32_t)ht;
}

/* ── renderer ────────────────────────────────────────────────────── */

int32_t aria_sdl3_create_renderer(int32_t win_h) {
    if (!g_initialized) return -1;
    if (win_h < 0 || win_h >= MAX_WINDOWS || !g_windows[win_h]) return -1;
    for (int32_t i = 0; i < MAX_RENDERERS; i++) {
        if (!g_renderers[i]) {
            g_renderers[i] = SDL_CreateRenderer(g_windows[win_h], NULL);
            if (!g_renderers[i]) return -1;
            return i;
        }
    }
    return -1;
}

void aria_sdl3_destroy_renderer(int32_t h) {
    if (h < 0 || h >= MAX_RENDERERS || !g_renderers[h]) return;
    SDL_DestroyRenderer(g_renderers[h]);
    g_renderers[h] = NULL;
}

/* ── drawing ─────────────────────────────────────────────────────── */

int32_t aria_sdl3_set_draw_color(int32_t rend, int32_t r, int32_t g, int32_t b, int32_t a) {
    if (rend < 0 || rend >= MAX_RENDERERS || !g_renderers[rend]) return -1;
    return SDL_SetRenderDrawColor(g_renderers[rend], (Uint8)r, (Uint8)g, (Uint8)b, (Uint8)a) ? 0 : -1;
}

int32_t aria_sdl3_clear(int32_t rend) {
    if (rend < 0 || rend >= MAX_RENDERERS || !g_renderers[rend]) return -1;
    return SDL_RenderClear(g_renderers[rend]) ? 0 : -1;
}

int32_t aria_sdl3_present(int32_t rend) {
    if (rend < 0 || rend >= MAX_RENDERERS || !g_renderers[rend]) return -1;
    return SDL_RenderPresent(g_renderers[rend]) ? 0 : -1;
}

int32_t aria_sdl3_draw_rect(int32_t rend, int32_t x, int32_t y, int32_t w, int32_t h) {
    if (rend < 0 || rend >= MAX_RENDERERS || !g_renderers[rend]) return -1;
    SDL_FRect rect = { (float)x, (float)y, (float)w, (float)h };
    return SDL_RenderFillRect(g_renderers[rend], &rect) ? 0 : -1;
}

int32_t aria_sdl3_draw_line(int32_t rend, int32_t x1, int32_t y1, int32_t x2, int32_t y2) {
    if (rend < 0 || rend >= MAX_RENDERERS || !g_renderers[rend]) return -1;
    return SDL_RenderLine(g_renderers[rend], (float)x1, (float)y1, (float)x2, (float)y2) ? 0 : -1;
}

int32_t aria_sdl3_draw_point(int32_t rend, int32_t x, int32_t y) {
    if (rend < 0 || rend >= MAX_RENDERERS || !g_renderers[rend]) return -1;
    return SDL_RenderPoint(g_renderers[rend], (float)x, (float)y) ? 0 : -1;
}

/* ── events ──────────────────────────────────────────────────────── */

int32_t aria_sdl3_poll_event(void) {
    return SDL_PollEvent(&g_last_event) ? 1 : 0;
}

int32_t aria_sdl3_event_type(void) {
    return (int32_t)g_last_event.type;
}

int32_t aria_sdl3_event_key_code(void) {
    if (g_last_event.type == SDL_EVENT_KEY_DOWN || g_last_event.type == SDL_EVENT_KEY_UP) {
        return (int32_t)g_last_event.key.key;
    }
    return 0;
}

/* ── timing ──────────────────────────────────────────────────────── */

int32_t aria_sdl3_get_ticks(void) {
    return (int32_t)(SDL_GetTicks() & 0x7FFFFFFF);
}

void aria_sdl3_delay(int32_t ms) {
    SDL_Delay((Uint32)ms);
}

/* ── version ─────────────────────────────────────────────────────── */

int32_t aria_sdl3_get_version(void) {
    return SDL_GetVersion();
}

/* ================================================================= */
/* ── test harness (non-graphical tests only) ─────────────────────── */
/* ================================================================= */

static int t_pass = 0, t_fail = 0;

static void assert_int_eq(int32_t a, int32_t b, const char *msg) {
    if (a == b) { t_pass++; printf("  PASS: %s\n", msg); }
    else        { t_fail++; printf("  FAIL: %s — got %d, expected %d\n", msg, a, b); }
}

static void assert_int_gt(int32_t a, int32_t b, const char *msg) {
    if (a > b)  { t_pass++; printf("  PASS: %s\n", msg); }
    else        { t_fail++; printf("  FAIL: %s — got %d, expected > %d\n", msg, a, b); }
}

static void assert_str_not_empty(const char *s, const char *msg) {
    if (s && strlen(s) > 0) { t_pass++; printf("  PASS: %s\n", msg); }
    else                    { t_fail++; printf("  FAIL: %s — got empty/null\n", msg); }
}

int32_t aria_test_sdl3_run(void) {
    printf("=== aria-sdl3 tests ===\n");

    /* pre-init state */
    assert_int_eq(aria_sdl3_is_init(), 0, "not initialized at start");

    /* init */
    int32_t r = aria_sdl3_init();
    assert_int_eq(r, 0, "init returns 0");
    assert_int_eq(aria_sdl3_is_init(), 1, "is_init = 1 after init");

    /* double init is safe */
    assert_int_eq(aria_sdl3_init(), 0, "double init returns 0");

    /* version */
    int32_t ver = aria_sdl3_get_version();
    assert_int_gt(ver, 0, "version > 0");

    /* create window */
    int32_t win = aria_sdl3_create_window("Test Window", 320, 240);
    assert_int_eq(win >= 0 ? 1 : 0, 1, "create_window returns valid handle");

    /* window dimensions */
    assert_int_eq(aria_sdl3_get_window_width(win), 320, "window width = 320");
    assert_int_eq(aria_sdl3_get_window_height(win), 240, "window height = 240");

    /* create renderer */
    int32_t rend = aria_sdl3_create_renderer(win);
    assert_int_eq(rend >= 0 ? 1 : 0, 1, "create_renderer returns valid handle");

    /* drawing operations (non-visual, just test return codes) */
    assert_int_eq(aria_sdl3_set_draw_color(rend, 255, 0, 0, 255), 0, "set draw color returns 0");
    assert_int_eq(aria_sdl3_clear(rend), 0, "clear returns 0");
    assert_int_eq(aria_sdl3_draw_rect(rend, 10, 10, 50, 50), 0, "draw_rect returns 0");
    assert_int_eq(aria_sdl3_draw_line(rend, 0, 0, 100, 100), 0, "draw_line returns 0");
    assert_int_eq(aria_sdl3_draw_point(rend, 50, 50), 0, "draw_point returns 0");
    assert_int_eq(aria_sdl3_present(rend), 0, "present returns 0");

    /* ticks */
    int32_t ticks = aria_sdl3_get_ticks();
    assert_int_eq(ticks >= 0 ? 1 : 0, 1, "get_ticks >= 0");

    /* invalid handles */
    assert_int_eq(aria_sdl3_set_draw_color(-1, 0, 0, 0, 0), -1, "invalid renderer returns -1");
    assert_int_eq(aria_sdl3_get_window_width(-1), 0, "invalid window width = 0");
    assert_int_eq(aria_sdl3_create_renderer(-1), -1, "renderer on invalid window = -1");

    /* event: no events pending in this context */
    int32_t ev = aria_sdl3_poll_event();
    /* can be 0 or 1 depending on system — just test the function works */
    assert_int_eq(ev == 0 || ev == 1 ? 1 : 0, 1, "poll_event returns 0 or 1");

    /* cleanup */
    aria_sdl3_destroy_renderer(rend);
    aria_sdl3_destroy_window(win);
    aria_sdl3_quit();
    assert_int_eq(aria_sdl3_is_init(), 0, "not initialized after quit");

    printf("\nResults: %d passed, %d failed (of %d)\n", t_pass, t_fail, t_pass + t_fail);
    return t_fail;
}

#ifdef ARIA_SDL3_TEST
int main(void) { return aria_test_sdl3_run(); }
#endif
