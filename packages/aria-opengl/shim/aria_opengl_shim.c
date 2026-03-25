/*  aria_opengl_shim.c — OpenGL 3.3 Core bridge for Aria FFI
 *
 *  Uses SDL2 for window/context management and GLAD for GL function loading.
 *  All OpenGL objects (shaders, programs, buffers, VAOs, textures) are
 *  returned as int32 handles that Aria can store and pass around.
 *
 *  Build:
 *    cc -O2 -shared -fPIC -Wall -o libaria_opengl_shim.so \
 *       aria_opengl_shim.c glad/src/glad.c \
 *       -Iglad/include $(pkg-config --cflags --libs sdl2 gl) -lm
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "glad/include/glad/glad.h"
#include <SDL2/SDL.h>

/* ── internal state ──────────────────────────────────────────────────── */

static SDL_Window   *g_window  = NULL;
static SDL_GLContext  g_context = NULL;

/* ── window + context ────────────────────────────────────────────────── */

int32_t aria_gl_init(int32_t width, int32_t height, const char *title) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) return -1;

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
                        SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    g_window = SDL_CreateWindow(
        title,
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        width, height,
        SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
    if (!g_window) return -2;

    g_context = SDL_GL_CreateContext(g_window);
    if (!g_context) { SDL_DestroyWindow(g_window); g_window = NULL; return -3; }

    if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
        SDL_GL_DeleteContext(g_context);
        SDL_DestroyWindow(g_window);
        g_context = NULL; g_window = NULL;
        return -4;
    }

    return 0;
}

void aria_gl_quit(void) {
    if (g_context) { SDL_GL_DeleteContext(g_context); g_context = NULL; }
    if (g_window)  { SDL_DestroyWindow(g_window); g_window = NULL; }
    SDL_Quit();
}

void aria_gl_swap(void) {
    if (g_window) SDL_GL_SwapWindow(g_window);
}

int32_t aria_gl_set_vsync(int32_t on) {
    return SDL_GL_SetSwapInterval(on) == 0 ? 0 : -1;
}

/* ── viewport + clear ────────────────────────────────────────────────── */

void aria_gl_viewport(int32_t x, int32_t y, int32_t w, int32_t h) {
    glViewport(x, y, w, h);
}

void aria_gl_clear_color(float r, float g, float b, float a) {
    glClearColor(r, g, b, a);
}

void aria_gl_clear(int32_t mask) {
    glClear((GLbitfield)mask);
}

void aria_gl_enable(int32_t cap) {
    glEnable((GLenum)cap);
}

void aria_gl_disable(int32_t cap) {
    glDisable((GLenum)cap);
}

/* ── shaders ─────────────────────────────────────────────────────────── */

int32_t aria_gl_create_shader(int32_t type) {
    return (int32_t)glCreateShader((GLenum)type);
}

void aria_gl_shader_source(int32_t shader, const char *source) {
    const GLchar *src = source;
    glShaderSource((GLuint)shader, 1, &src, NULL);
}

void aria_gl_compile_shader(int32_t shader) {
    glCompileShader((GLuint)shader);
}

int32_t aria_gl_get_shader_status(int32_t shader) {
    GLint success;
    glGetShaderiv((GLuint)shader, GL_COMPILE_STATUS, &success);
    return success ? 1 : 0;
}

int32_t aria_gl_get_shader_log(int32_t shader, char *buf, int32_t bufsize) {
    GLsizei len = 0;
    glGetShaderInfoLog((GLuint)shader, bufsize, &len, buf);
    return (int32_t)len;
}

void aria_gl_print_shader_log(int32_t shader) {
    char buf[1024];
    GLsizei len = 0;
    glGetShaderInfoLog((GLuint)shader, 1024, &len, buf);
    if (len > 0) fprintf(stderr, "Shader log: %s\n", buf);
}

void aria_gl_print_program_log(int32_t program) {
    char buf[1024];
    GLsizei len = 0;
    glGetProgramInfoLog((GLuint)program, 1024, &len, buf);
    if (len > 0) fprintf(stderr, "Program log: %s\n", buf);
}

void aria_gl_delete_shader(int32_t shader) {
    glDeleteShader((GLuint)shader);
}

/* ── programs ────────────────────────────────────────────────────────── */

int32_t aria_gl_create_program(void) {
    return (int32_t)glCreateProgram();
}

void aria_gl_attach_shader(int32_t program, int32_t shader) {
    glAttachShader((GLuint)program, (GLuint)shader);
}

void aria_gl_link_program(int32_t program) {
    glLinkProgram((GLuint)program);
}

int32_t aria_gl_get_program_status(int32_t program) {
    GLint success;
    glGetProgramiv((GLuint)program, GL_LINK_STATUS, &success);
    return success ? 1 : 0;
}

void aria_gl_use_program(int32_t program) {
    glUseProgram((GLuint)program);
}

void aria_gl_delete_program(int32_t program) {
    glDeleteProgram((GLuint)program);
}

/* ── compile + link helper ───────────────────────────────────────────── */

/* Compiles vert + frag source and returns linked program handle.
   Returns -1 on failure (check logs). */
int32_t aria_gl_build_program(const char *vert_src, const char *frag_src) {
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vert_src, NULL);
    glCompileShader(vs);
    GLint ok;
    glGetShaderiv(vs, GL_COMPILE_STATUS, &ok);
    if (!ok) { glDeleteShader(vs); return -1; }

    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &frag_src, NULL);
    glCompileShader(fs);
    glGetShaderiv(fs, GL_COMPILE_STATUS, &ok);
    if (!ok) { glDeleteShader(vs); glDeleteShader(fs); return -2; }

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    glDeleteShader(vs);
    glDeleteShader(fs);
    if (!ok) { glDeleteProgram(prog); return -3; }

    return (int32_t)prog;
}

/* ── buffers ─────────────────────────────────────────────────────────── */

int32_t aria_gl_gen_vao(void) {
    GLuint vao;
    glGenVertexArrays(1, &vao);
    return (int32_t)vao;
}

void aria_gl_bind_vao(int32_t vao) {
    glBindVertexArray((GLuint)vao);
}

void aria_gl_delete_vao(int32_t vao) {
    GLuint v = (GLuint)vao;
    glDeleteVertexArrays(1, &v);
}

int32_t aria_gl_gen_buffer(void) {
    GLuint buf;
    glGenBuffers(1, &buf);
    return (int32_t)buf;
}

void aria_gl_bind_buffer(int32_t target, int32_t buffer) {
    glBindBuffer((GLenum)target, (GLuint)buffer);
}

void aria_gl_delete_buffer(int32_t buffer) {
    GLuint b = (GLuint)buffer;
    glDeleteBuffers(1, &b);
}

/* ── vertex data upload ──────────────────────────────────────────────── */

/* Upload float array data.  Since Aria can't pass float arrays, we provide
   pre-built geometry helpers and a raw upload that takes a C-side buffer. */

/* Internal scratch buffer for building geometry from Aria scalar calls */
#define ARIA_GL_MAX_FLOATS 8192
static float g_float_buf[ARIA_GL_MAX_FLOATS];
static int32_t g_float_count = 0;

void aria_gl_buf_clear(void) {
    g_float_count = 0;
}

void aria_gl_buf_push(float val) {
    if (g_float_count < ARIA_GL_MAX_FLOATS)
        g_float_buf[g_float_count++] = val;
}

void aria_gl_buf_push3(float a, float b, float c) {
    if (g_float_count + 3 <= ARIA_GL_MAX_FLOATS) {
        g_float_buf[g_float_count++] = a;
        g_float_buf[g_float_count++] = b;
        g_float_buf[g_float_count++] = c;
    }
}

void aria_gl_buf_push6(float a, float b, float c, float d, float e, float f) {
    if (g_float_count + 6 <= ARIA_GL_MAX_FLOATS) {
        g_float_buf[g_float_count++] = a;
        g_float_buf[g_float_count++] = b;
        g_float_buf[g_float_count++] = c;
        g_float_buf[g_float_count++] = d;
        g_float_buf[g_float_count++] = e;
        g_float_buf[g_float_count++] = f;
    }
}

int32_t aria_gl_buf_count(void) {
    return g_float_count;
}

void aria_gl_buffer_data_f(int32_t target, int32_t usage) {
    glBufferData((GLenum)target,
                 (GLsizeiptr)(g_float_count * sizeof(float)),
                 g_float_buf, (GLenum)usage);
}

/* ── vertex attributes ───────────────────────────────────────────────── */

void aria_gl_vertex_attrib_pointer(int32_t index, int32_t size,
                                   int32_t stride_floats, int32_t offset_floats) {
    glVertexAttribPointer(
        (GLuint)index, size, GL_FLOAT, GL_FALSE,
        stride_floats * (int32_t)sizeof(float),
        (const void *)(intptr_t)(offset_floats * (int32_t)sizeof(float)));
}

void aria_gl_enable_vertex_attrib(int32_t index) {
    glEnableVertexAttribArray((GLuint)index);
}

void aria_gl_disable_vertex_attrib(int32_t index) {
    glDisableVertexAttribArray((GLuint)index);
}

/* ── drawing ─────────────────────────────────────────────────────────── */

void aria_gl_draw_arrays(int32_t mode, int32_t first, int32_t count) {
    glDrawArrays((GLenum)mode, first, count);
}

void aria_gl_draw_elements(int32_t mode, int32_t count, int32_t type,
                           int32_t offset) {
    glDrawElements((GLenum)mode, count, (GLenum)type,
                   (const void *)(intptr_t)offset);
}

/* ── uniforms ────────────────────────────────────────────────────────── */

int32_t aria_gl_get_uniform_location(int32_t program, const char *name) {
    return (int32_t)glGetUniformLocation((GLuint)program, name);
}

void aria_gl_uniform1i(int32_t loc, int32_t val) {
    glUniform1i(loc, val);
}

void aria_gl_uniform1f(int32_t loc, float val) {
    glUniform1f(loc, val);
}

void aria_gl_uniform3f(int32_t loc, float x, float y, float z) {
    glUniform3f(loc, x, y, z);
}

void aria_gl_uniform4f(int32_t loc, float x, float y, float z, float w) {
    glUniform4f(loc, x, y, z, w);
}

/* Upload current float buffer as a 4x4 matrix uniform (16 floats). */
void aria_gl_uniform_matrix4fv(int32_t loc) {
    if (g_float_count >= 16)
        glUniformMatrix4fv(loc, 1, GL_FALSE, g_float_buf);
}

/* Upload matrix from 16 explicit floats (avoids needing the buffer). */
void aria_gl_uniform_mat4(int32_t loc,
    float m0,  float m1,  float m2,  float m3,
    float m4,  float m5,  float m6,  float m7,
    float m8,  float m9,  float m10, float m11,
    float m12, float m13, float m14, float m15) {
    float m[16] = { m0,m1,m2,m3, m4,m5,m6,m7, m8,m9,m10,m11, m12,m13,m14,m15 };
    glUniformMatrix4fv(loc, 1, GL_FALSE, m);
}

/* ── textures ────────────────────────────────────────────────────────── */

int32_t aria_gl_gen_texture(void) {
    GLuint tex;
    glGenTextures(1, &tex);
    return (int32_t)tex;
}

void aria_gl_bind_texture(int32_t target, int32_t texture) {
    glBindTexture((GLenum)target, (GLuint)texture);
}

void aria_gl_tex_parameter_i(int32_t target, int32_t pname, int32_t param) {
    glTexParameteri((GLenum)target, (GLenum)pname, param);
}

void aria_gl_delete_texture(int32_t texture) {
    GLuint t = (GLuint)texture;
    glDeleteTextures(1, &t);
}

/* ── matrix helpers (C-side 4×4 math for the cube demo) ──────────────── */

/* These build a 4x4 matrix in g_float_buf (column-major) so Aria can
   upload it via aria_gl_uniform_matrix4fv(). */

static void mat4_identity(float *m) {
    memset(m, 0, 16 * sizeof(float));
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

void aria_gl_mat4_identity(void) {
    g_float_count = 16;
    mat4_identity(g_float_buf);
}

void aria_gl_mat4_perspective(float fov_deg, float aspect, float near_val, float far_val) {
    g_float_count = 16;
    memset(g_float_buf, 0, 16 * sizeof(float));
    float f = 1.0f / tanf(fov_deg * 3.14159265f / 360.0f);
    g_float_buf[0]  = f / aspect;
    g_float_buf[5]  = f;
    g_float_buf[10] = (far_val + near_val) / (near_val - far_val);
    g_float_buf[11] = -1.0f;
    g_float_buf[14] = (2.0f * far_val * near_val) / (near_val - far_val);
}

void aria_gl_mat4_translate(float x, float y, float z) {
    g_float_count = 16;
    mat4_identity(g_float_buf);
    g_float_buf[12] = x;
    g_float_buf[13] = y;
    g_float_buf[14] = z;
}

void aria_gl_mat4_rotate_y(float angle_deg) {
    g_float_count = 16;
    mat4_identity(g_float_buf);
    float c = cosf(angle_deg * 3.14159265f / 180.0f);
    float s = sinf(angle_deg * 3.14159265f / 180.0f);
    g_float_buf[0]  =  c;
    g_float_buf[2]  =  s;
    g_float_buf[8]  = -s;
    g_float_buf[10] =  c;
}

void aria_gl_mat4_rotate_x(float angle_deg) {
    g_float_count = 16;
    mat4_identity(g_float_buf);
    float c = cosf(angle_deg * 3.14159265f / 180.0f);
    float s = sinf(angle_deg * 3.14159265f / 180.0f);
    g_float_buf[5]  =  c;
    g_float_buf[6]  =  s;
    g_float_buf[9]  = -s;
    g_float_buf[10] =  c;
}

/* Multiply two 4x4 matrices: result = A * B (column-major).
   A is in g_float_buf[0..15], B is in g_float_buf[16..31].
   Result overwrites g_float_buf[0..15], g_float_count set to 16. */
void aria_gl_mat4_multiply(void) {
    if (g_float_count < 32) return;
    float *a = g_float_buf;
    float *b = g_float_buf + 16;
    float r[16];
    for (int col = 0; col < 4; col++) {
        for (int row = 0; row < 4; row++) {
            r[col*4+row] = a[0*4+row]*b[col*4+0]
                         + a[1*4+row]*b[col*4+1]
                         + a[2*4+row]*b[col*4+2]
                         + a[3*4+row]*b[col*4+3];
        }
    }
    memcpy(g_float_buf, r, 16 * sizeof(float));
    g_float_count = 16;
}

/* Store current 16-float matrix (g_float_buf[0..15]) into slot 0 or 1.
   Used to store intermediate matrices for multiplication. */
static float g_mat_slot[2][16];

void aria_gl_mat4_store(int32_t slot) {
    if (slot >= 0 && slot <= 1 && g_float_count >= 16)
        memcpy(g_mat_slot[slot], g_float_buf, 16 * sizeof(float));
}

void aria_gl_mat4_load(int32_t slot) {
    if (slot >= 0 && slot <= 1) {
        memcpy(g_float_buf, g_mat_slot[slot], 16 * sizeof(float));
        g_float_count = 16;
    }
}

/* Multiply slot0 * slot1, result in g_float_buf[0..15]. */
void aria_gl_mat4_mul_slots(int32_t a_slot, int32_t b_slot) {
    if (a_slot < 0 || a_slot > 1 || b_slot < 0 || b_slot > 1) return;
    float *a = g_mat_slot[a_slot];
    float *b = g_mat_slot[b_slot];
    float r[16];
    for (int col = 0; col < 4; col++) {
        for (int row = 0; row < 4; row++) {
            r[col*4+row] = a[0*4+row]*b[col*4+0]
                         + a[1*4+row]*b[col*4+1]
                         + a[2*4+row]*b[col*4+2]
                         + a[3*4+row]*b[col*4+3];
        }
    }
    memcpy(g_float_buf, r, 16 * sizeof(float));
    g_float_count = 16;
}

/* ── SDL2 event forwarding ───────────────────────────────────────────── */

static SDL_Event g_event;

int32_t aria_gl_poll_event(void) {
    return SDL_PollEvent(&g_event);
}

int32_t aria_gl_event_type(void) {
    return (int32_t)g_event.type;
}

int32_t aria_gl_event_key_scancode(void) {
    return (int32_t)g_event.key.keysym.scancode;
}

int32_t aria_gl_event_key_keycode(void) {
    return (int32_t)g_event.key.keysym.sym;
}

int32_t aria_gl_get_ticks(void) {
    return (int32_t)SDL_GetTicks();
}

void aria_gl_delay(int32_t ms) {
    SDL_Delay((Uint32)ms);
}

/* ── OpenGL constants ────────────────────────────────────────────────── */

int32_t aria_gl_COLOR_BUFFER_BIT(void)   { return GL_COLOR_BUFFER_BIT; }
int32_t aria_gl_DEPTH_BUFFER_BIT(void)   { return GL_DEPTH_BUFFER_BIT; }
int32_t aria_gl_DEPTH_TEST(void)         { return GL_DEPTH_TEST; }
int32_t aria_gl_VERTEX_SHADER(void)      { return GL_VERTEX_SHADER; }
int32_t aria_gl_FRAGMENT_SHADER(void)    { return GL_FRAGMENT_SHADER; }
int32_t aria_gl_ARRAY_BUFFER(void)       { return GL_ARRAY_BUFFER; }
int32_t aria_gl_ELEMENT_ARRAY_BUFFER(void) { return GL_ELEMENT_ARRAY_BUFFER; }
int32_t aria_gl_STATIC_DRAW(void)        { return GL_STATIC_DRAW; }
int32_t aria_gl_DYNAMIC_DRAW(void)       { return GL_DYNAMIC_DRAW; }
int32_t aria_gl_TRIANGLES(void)          { return GL_TRIANGLES; }
int32_t aria_gl_LINES(void)              { return GL_LINES; }
int32_t aria_gl_POINTS(void)             { return GL_POINTS; }
int32_t aria_gl_UNSIGNED_INT(void)       { return GL_UNSIGNED_INT; }
int32_t aria_gl_UNSIGNED_SHORT(void)     { return GL_UNSIGNED_SHORT; }
int32_t aria_gl_TEXTURE_2D(void)         { return GL_TEXTURE_2D; }
int32_t aria_gl_TEXTURE_MIN_FILTER(void) { return GL_TEXTURE_MIN_FILTER; }
int32_t aria_gl_TEXTURE_MAG_FILTER(void) { return GL_TEXTURE_MAG_FILTER; }
int32_t aria_gl_LINEAR(void)             { return GL_LINEAR; }
int32_t aria_gl_NEAREST(void)            { return GL_NEAREST; }
int32_t aria_gl_CULL_FACE(void)          { return GL_CULL_FACE; }
int32_t aria_gl_BLEND(void)              { return GL_BLEND; }

/* ── pre-built cube geometry ─────────────────────────────────────────── */

/* Loads 36 vertices × 6 floats (pos xyz + color rgb) into g_float_buf.
   Call this, then aria_gl_buffer_data_f() to upload. */
void aria_gl_load_cube_geometry(void) {
    /* Each face: 2 triangles × 3 verts × 6 floats (x,y,z, r,g,b) */
    static const float cube[] = {
        /* Front face (red) */
        -0.5f,-0.5f, 0.5f,  1,0,0,   0.5f,-0.5f, 0.5f,  1,0,0,   0.5f, 0.5f, 0.5f,  1,0,0,
        -0.5f,-0.5f, 0.5f,  1,0,0,   0.5f, 0.5f, 0.5f,  1,0,0,  -0.5f, 0.5f, 0.5f,  1,0,0,
        /* Back face (green) */
         0.5f,-0.5f,-0.5f,  0,1,0,  -0.5f,-0.5f,-0.5f,  0,1,0,  -0.5f, 0.5f,-0.5f,  0,1,0,
         0.5f,-0.5f,-0.5f,  0,1,0,  -0.5f, 0.5f,-0.5f,  0,1,0,   0.5f, 0.5f,-0.5f,  0,1,0,
        /* Left face (blue) */
        -0.5f,-0.5f,-0.5f,  0,0,1,  -0.5f,-0.5f, 0.5f,  0,0,1,  -0.5f, 0.5f, 0.5f,  0,0,1,
        -0.5f,-0.5f,-0.5f,  0,0,1,  -0.5f, 0.5f, 0.5f,  0,0,1,  -0.5f, 0.5f,-0.5f,  0,0,1,
        /* Right face (yellow) */
         0.5f,-0.5f, 0.5f,  1,1,0,   0.5f,-0.5f,-0.5f,  1,1,0,   0.5f, 0.5f,-0.5f,  1,1,0,
         0.5f,-0.5f, 0.5f,  1,1,0,   0.5f, 0.5f,-0.5f,  1,1,0,   0.5f, 0.5f, 0.5f,  1,1,0,
        /* Top face (cyan) */
        -0.5f, 0.5f, 0.5f,  0,1,1,   0.5f, 0.5f, 0.5f,  0,1,1,   0.5f, 0.5f,-0.5f,  0,1,1,
        -0.5f, 0.5f, 0.5f,  0,1,1,   0.5f, 0.5f,-0.5f,  0,1,1,  -0.5f, 0.5f,-0.5f,  0,1,1,
        /* Bottom face (magenta) */
        -0.5f,-0.5f,-0.5f,  1,0,1,   0.5f,-0.5f,-0.5f,  1,0,1,   0.5f,-0.5f, 0.5f,  1,0,1,
        -0.5f,-0.5f,-0.5f,  1,0,1,   0.5f,-0.5f, 0.5f,  1,0,1,  -0.5f,-0.5f, 0.5f,  1,0,1,
    };
    g_float_count = 36 * 6;
    memcpy(g_float_buf, cube, sizeof(cube));
}

/* ── diagnostics ─────────────────────────────────────────────────────── */

int32_t aria_gl_get_error(void) {
    return (int32_t)glGetError();
}

void aria_gl_debug_matrix(void) {
    fprintf(stderr, "g_float_buf matrix (count=%d):\n", g_float_count);
    if (g_float_count >= 16) {
        for (int i = 0; i < 4; i++) {
            fprintf(stderr, "  [%7.4f %7.4f %7.4f %7.4f]\n",
                    g_float_buf[i], g_float_buf[4+i], g_float_buf[8+i], g_float_buf[12+i]);
        }
    }
}

