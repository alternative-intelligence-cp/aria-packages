/* aria_gml_shim.c — GML compatibility layer over raylib
 *
 * Provides GameMaker-style function names and semantics atop raylib.
 * Key differences from raw raylib:
 *   - GML uses a "current draw color" that persists across calls
 *   - GML rectangles use (x1,y1)-(x2,y2) corners, not (x,y,w,h)
 *   - GML colors are packed as  r | (g << 8) | (b << 16)
 *   - GML uses outline param: 0 = filled, 1 = outline
 */

#include <raylib.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>

/* ── draw state ─────────────────────────────────────────────────────── */

static uint8_t g_draw_r = 255;
static uint8_t g_draw_g = 255;
static uint8_t g_draw_b = 255;
static uint8_t g_draw_a = 255;
static int32_t g_font_size = 20;

static inline Color draw_color(void) {
    return (Color){ g_draw_r, g_draw_g, g_draw_b, g_draw_a };
}

/* ── color helpers ──────────────────────────────────────────────────── */

/* GML packs colors as r | (g<<8) | (b<<16), no alpha channel. */
int32_t aria_gml_make_color_rgb(int32_t r, int32_t g, int32_t b) {
    return (r & 0xFF) | ((g & 0xFF) << 8) | ((b & 0xFF) << 16);
}

int32_t aria_gml_color_get_red(int32_t col)   { return col & 0xFF; }
int32_t aria_gml_color_get_green(int32_t col) { return (col >> 8) & 0xFF; }
int32_t aria_gml_color_get_blue(int32_t col)  { return (col >> 16) & 0xFF; }

/* ── draw state setters ─────────────────────────────────────────────── */

void aria_gml_draw_set_color(int32_t col) {
    g_draw_r = (uint8_t)(col & 0xFF);
    g_draw_g = (uint8_t)((col >> 8) & 0xFF);
    g_draw_b = (uint8_t)((col >> 16) & 0xFF);
}

void aria_gml_draw_set_alpha(float alpha) {
    if (alpha < 0.0f) alpha = 0.0f;
    if (alpha > 1.0f) alpha = 1.0f;
    g_draw_a = (uint8_t)(alpha * 255.0f);
}

int32_t aria_gml_draw_get_color(void) {
    return g_draw_r | (g_draw_g << 8) | (g_draw_b << 16);
}

float aria_gml_draw_get_alpha(void) {
    return (float)g_draw_a / 255.0f;
}

void aria_gml_draw_set_font_size(int32_t size) {
    if (size < 1) size = 1;
    g_font_size = size;
}

/* ── game loop ──────────────────────────────────────────────────────── */

void aria_gml_game_init(int32_t width, int32_t height, const char *title) {
    InitWindow(width, height, title);
    InitAudioDevice();
}

int32_t aria_gml_game_should_close(void) {
    return WindowShouldClose() ? 1 : 0;
}

void aria_gml_game_begin_step(void) {
    /* placeholder for frame-start bookkeeping (timers, etc.) */
}

void aria_gml_game_begin_draw(void) {
    BeginDrawing();
}

void aria_gml_game_end_draw(void) {
    EndDrawing();
}

void aria_gml_game_shutdown(void) {
    CloseAudioDevice();
    CloseWindow();
}

void aria_gml_game_set_target_fps(int32_t fps) {
    SetTargetFPS(fps);
}

int32_t aria_gml_game_get_fps(void) {
    return GetFPS();
}

float aria_gml_game_get_time(void) {
    return (float)GetTime();
}

float aria_gml_game_get_frame_time(void) {
    return GetFrameTime();
}

/* ── drawing ────────────────────────────────────────────────────────── */

void aria_gml_draw_clear(int32_t col) {
    Color c = { (uint8_t)(col & 0xFF),
                (uint8_t)((col >> 8) & 0xFF),
                (uint8_t)((col >> 16) & 0xFF), 255 };
    ClearBackground(c);
}

void aria_gml_draw_rectangle(float x1, float y1, float x2, float y2,
                             int32_t outline) {
    float x = fminf(x1, x2);
    float y = fminf(y1, y2);
    float w = fabsf(x2 - x1);
    float h = fabsf(y2 - y1);
    Color c = draw_color();
    if (outline) {
        DrawRectangleLinesEx((Rectangle){x, y, w, h}, 1.0f, c);
    } else {
        DrawRectangle((int)x, (int)y, (int)w, (int)h, c);
    }
}

void aria_gml_draw_circle(float x, float y, float radius, int32_t outline) {
    Color c = draw_color();
    if (outline) {
        DrawCircleLines((int)x, (int)y, radius, c);
    } else {
        DrawCircle((int)x, (int)y, radius, c);
    }
}

void aria_gml_draw_triangle(float x1, float y1, float x2, float y2,
                            float x3, float y3, int32_t outline) {
    Color c = draw_color();
    Vector2 v1 = {x1, y1}, v2 = {x2, y2}, v3 = {x3, y3};
    if (outline) {
        DrawTriangleLines(v1, v2, v3, c);
    } else {
        DrawTriangle(v1, v2, v3, c);
    }
}

void aria_gml_draw_line(float x1, float y1, float x2, float y2) {
    DrawLine((int)x1, (int)y1, (int)x2, (int)y2, draw_color());
}

void aria_gml_draw_line_width(float x1, float y1, float x2, float y2,
                              float width) {
    DrawLineEx((Vector2){x1, y1}, (Vector2){x2, y2}, width, draw_color());
}

void aria_gml_draw_point(float x, float y) {
    DrawPixel((int)x, (int)y, draw_color());
}

void aria_gml_draw_text(float x, float y, const char *text) {
    DrawText(text, (int)x, (int)y, g_font_size, draw_color());
}

int32_t aria_gml_string_width(const char *text) {
    return MeasureText(text, g_font_size);
}

void aria_gml_draw_text_ext(float x, float y, const char *text,
                            int32_t font_size) {
    DrawText(text, (int)x, (int)y, font_size, draw_color());
}

/* ── sprites (textures) ─────────────────────────────────────────────── */

#define GML_MAX_SPRITES 256
static Texture2D g_sprites[GML_MAX_SPRITES];
static int32_t   g_sprite_used[GML_MAX_SPRITES];

int32_t aria_gml_sprite_load(const char *path) {
    for (int32_t i = 0; i < GML_MAX_SPRITES; i++) {
        if (!g_sprite_used[i]) {
            g_sprites[i] = LoadTexture(path);
            if (g_sprites[i].id == 0) return -1;
            g_sprite_used[i] = 1;
            return i;
        }
    }
    return -1;
}

void aria_gml_sprite_delete(int32_t handle) {
    if (handle >= 0 && handle < GML_MAX_SPRITES && g_sprite_used[handle]) {
        UnloadTexture(g_sprites[handle]);
        g_sprite_used[handle] = 0;
    }
}

int32_t aria_gml_sprite_get_width(int32_t handle) {
    if (handle >= 0 && handle < GML_MAX_SPRITES && g_sprite_used[handle])
        return g_sprites[handle].width;
    return 0;
}

int32_t aria_gml_sprite_get_height(int32_t handle) {
    if (handle >= 0 && handle < GML_MAX_SPRITES && g_sprite_used[handle])
        return g_sprites[handle].height;
    return 0;
}

void aria_gml_draw_sprite(int32_t handle, float x, float y) {
    if (handle >= 0 && handle < GML_MAX_SPRITES && g_sprite_used[handle]) {
        DrawTexture(g_sprites[handle], (int)x, (int)y, draw_color());
    }
}

void aria_gml_draw_sprite_ext(int32_t handle, float x, float y,
                              float xscale, float yscale, float rot) {
    if (handle < 0 || handle >= GML_MAX_SPRITES || !g_sprite_used[handle])
        return;
    Texture2D tex = g_sprites[handle];
    Rectangle src = { 0, 0, (float)tex.width, (float)tex.height };
    Rectangle dst = { x, y,
                      (float)tex.width  * fabsf(xscale),
                      (float)tex.height * fabsf(yscale) };
    Vector2 origin = { 0, 0 };
    DrawTexturePro(tex, src, dst, origin, rot, draw_color());
}

/* ── sounds ─────────────────────────────────────────────────────────── */

#define GML_MAX_SOUNDS 128
static Sound   g_sounds[GML_MAX_SOUNDS];
static int32_t g_sound_used[GML_MAX_SOUNDS];

int32_t aria_gml_sound_load(const char *path) {
    for (int32_t i = 0; i < GML_MAX_SOUNDS; i++) {
        if (!g_sound_used[i]) {
            g_sounds[i] = LoadSound(path);
            if (g_sounds[i].frameCount == 0) return -1;
            g_sound_used[i] = 1;
            return i;
        }
    }
    return -1;
}

void aria_gml_sound_delete(int32_t handle) {
    if (handle >= 0 && handle < GML_MAX_SOUNDS && g_sound_used[handle]) {
        UnloadSound(g_sounds[handle]);
        g_sound_used[handle] = 0;
    }
}

void aria_gml_audio_play_sound(int32_t handle) {
    if (handle >= 0 && handle < GML_MAX_SOUNDS && g_sound_used[handle])
        PlaySound(g_sounds[handle]);
}

void aria_gml_audio_stop_sound(int32_t handle) {
    if (handle >= 0 && handle < GML_MAX_SOUNDS && g_sound_used[handle])
        StopSound(g_sounds[handle]);
}

int32_t aria_gml_audio_is_playing(int32_t handle) {
    if (handle >= 0 && handle < GML_MAX_SOUNDS && g_sound_used[handle])
        return IsSoundPlaying(g_sounds[handle]) ? 1 : 0;
    return 0;
}

void aria_gml_audio_set_volume(int32_t handle, float vol) {
    if (handle >= 0 && handle < GML_MAX_SOUNDS && g_sound_used[handle])
        SetSoundVolume(g_sounds[handle], vol);
}

/* ── music ──────────────────────────────────────────────────────────── */

#define GML_MAX_MUSIC 32
static Music   g_music[GML_MAX_MUSIC];
static int32_t g_music_used[GML_MAX_MUSIC];

int32_t aria_gml_music_load(const char *path) {
    for (int32_t i = 0; i < GML_MAX_MUSIC; i++) {
        if (!g_music_used[i]) {
            g_music[i] = LoadMusicStream(path);
            if (g_music[i].frameCount == 0) return -1;
            g_music_used[i] = 1;
            return i;
        }
    }
    return -1;
}

void aria_gml_music_delete(int32_t handle) {
    if (handle >= 0 && handle < GML_MAX_MUSIC && g_music_used[handle]) {
        UnloadMusicStream(g_music[handle]);
        g_music_used[handle] = 0;
    }
}

void aria_gml_music_play(int32_t handle) {
    if (handle >= 0 && handle < GML_MAX_MUSIC && g_music_used[handle])
        PlayMusicStream(g_music[handle]);
}

void aria_gml_music_stop(int32_t handle) {
    if (handle >= 0 && handle < GML_MAX_MUSIC && g_music_used[handle])
        StopMusicStream(g_music[handle]);
}

void aria_gml_music_update(int32_t handle) {
    if (handle >= 0 && handle < GML_MAX_MUSIC && g_music_used[handle])
        UpdateMusicStream(g_music[handle]);
}

void aria_gml_music_set_volume(int32_t handle, float vol) {
    if (handle >= 0 && handle < GML_MAX_MUSIC && g_music_used[handle])
        SetMusicVolume(g_music[handle], vol);
}

int32_t aria_gml_music_is_playing(int32_t handle) {
    if (handle >= 0 && handle < GML_MAX_MUSIC && g_music_used[handle])
        return IsMusicStreamPlaying(g_music[handle]) ? 1 : 0;
    return 0;
}

/* ── keyboard input ─────────────────────────────────────────────────── */

int32_t aria_gml_keyboard_check(int32_t key) {
    return IsKeyDown(key) ? 1 : 0;
}

int32_t aria_gml_keyboard_check_pressed(int32_t key) {
    return IsKeyPressed(key) ? 1 : 0;
}

int32_t aria_gml_keyboard_check_released(int32_t key) {
    return IsKeyReleased(key) ? 1 : 0;
}

int32_t aria_gml_keyboard_lastkey(void) {
    return GetKeyPressed();
}

/* ── mouse input ────────────────────────────────────────────────────── */

int32_t aria_gml_mouse_x(void) { return GetMouseX(); }
int32_t aria_gml_mouse_y(void) { return GetMouseY(); }

int32_t aria_gml_mouse_check_button(int32_t btn) {
    return IsMouseButtonDown(btn) ? 1 : 0;
}

int32_t aria_gml_mouse_check_button_pressed(int32_t btn) {
    return IsMouseButtonPressed(btn) ? 1 : 0;
}

int32_t aria_gml_mouse_check_button_released(int32_t btn) {
    return IsMouseButtonReleased(btn) ? 1 : 0;
}

float aria_gml_mouse_wheel(void) {
    return GetMouseWheelMove();
}

/* ── random (simple xorshift32) ─────────────────────────────────────── */

static uint32_t g_rng_state = 1;

void aria_gml_randomize(int32_t seed) {
    g_rng_state = (uint32_t)(seed ? seed : 1);
}

static uint32_t xorshift32(void) {
    uint32_t x = g_rng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    g_rng_state = x;
    return x;
}

int32_t aria_gml_irandom(int32_t max) {
    if (max <= 0) return 0;
    return (int32_t)(xorshift32() % ((uint32_t)max + 1));
}

int32_t aria_gml_irandom_range(int32_t lo, int32_t hi) {
    if (hi < lo) { int32_t t = lo; lo = hi; hi = t; }
    uint32_t range = (uint32_t)(hi - lo + 1);
    return lo + (int32_t)(xorshift32() % range);
}

float aria_gml_random(float max) {
    return max * ((float)xorshift32() / 4294967295.0f);
}

float aria_gml_random_range(float lo, float hi) {
    return lo + (hi - lo) * ((float)xorshift32() / 4294967295.0f);
}

/* ── window queries ─────────────────────────────────────────────────── */

int32_t aria_gml_window_get_width(void)  { return GetScreenWidth(); }
int32_t aria_gml_window_get_height(void) { return GetScreenHeight(); }
