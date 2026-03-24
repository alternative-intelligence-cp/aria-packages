/* aria_cli_shim.c — ANSI terminal colors, styles, and formatting */
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#define MAX_BUF 8192

static char result[MAX_BUF];

/* ---- color text ---- */
static const char *wrap_ansi(const char *text, const char *code) {
    snprintf(result, MAX_BUF, "\033[%sm%s\033[0m", code, text);
    return result;
}

/* Foreground colors */
const char *aria_cli_red(const char *text)     { return wrap_ansi(text, "31"); }
const char *aria_cli_green(const char *text)   { return wrap_ansi(text, "32"); }
const char *aria_cli_yellow(const char *text)  { return wrap_ansi(text, "33"); }
const char *aria_cli_blue(const char *text)    { return wrap_ansi(text, "34"); }
const char *aria_cli_magenta(const char *text) { return wrap_ansi(text, "35"); }
const char *aria_cli_cyan(const char *text)    { return wrap_ansi(text, "36"); }
const char *aria_cli_white(const char *text)   { return wrap_ansi(text, "37"); }
const char *aria_cli_gray(const char *text)    { return wrap_ansi(text, "90"); }

/* Styles */
const char *aria_cli_bold(const char *text)      { return wrap_ansi(text, "1"); }
const char *aria_cli_dim(const char *text)       { return wrap_ansi(text, "2"); }
const char *aria_cli_italic(const char *text)    { return wrap_ansi(text, "3"); }
const char *aria_cli_underline(const char *text) { return wrap_ansi(text, "4"); }
const char *aria_cli_strikethrough(const char *text) { return wrap_ansi(text, "9"); }

/* Bright colors */
const char *aria_cli_bright_red(const char *text)   { return wrap_ansi(text, "91"); }
const char *aria_cli_bright_green(const char *text) { return wrap_ansi(text, "92"); }
const char *aria_cli_bright_blue(const char *text)  { return wrap_ansi(text, "94"); }

/* ---- progress bar ---- */
const char *aria_cli_progress(int32_t current, int32_t total, int32_t width) {
    if (total <= 0) total = 1;
    if (width <= 0) width = 40;
    if (width > 200) width = 200;
    int filled = (int)((long)current * width / total);
    if (filled > width) filled = width;
    if (filled < 0) filled = 0;

    char *p = result;
    *p++ = '[';
    for (int i = 0; i < width; i++) {
        if (i < filled) *p++ = '#';
        else *p++ = '-';
    }
    int pct = (int)((long)current * 100 / total);
    if (pct > 100) pct = 100;
    if (pct < 0) pct = 0;
    p += sprintf(p, "] %d%%", pct);
    *p = '\0';
    return result;
}

/* ---- horizontal rule ---- */
const char *aria_cli_hr(int32_t width, const char *ch) {
    if (width <= 0) width = 40;
    if (width >= MAX_BUF) width = MAX_BUF - 1;
    char c = (ch && *ch) ? *ch : '-';
    for (int i = 0; i < width; i++) result[i] = c;
    result[width] = '\0';
    return result;
}

/* ---- banner (centered text in a box) ---- */
const char *aria_cli_banner(const char *text, int32_t width) {
    if (width <= 0) width = 60;
    if (width >= (MAX_BUF / 4)) width = (MAX_BUF / 4);
    int tlen = (int)strlen(text);
    int pad = (width - tlen - 2) / 2;
    if (pad < 0) pad = 0;

    char *p = result;
    /* top border */
    *p++ = '+';
    for (int i = 0; i < width - 2; i++) *p++ = '-';
    *p++ = '+';
    *p++ = '\n';
    /* text line */
    *p++ = '|';
    for (int i = 0; i < pad; i++) *p++ = ' ';
    memcpy(p, text, (size_t)tlen);
    p += tlen;
    int rpad = width - 2 - pad - tlen;
    for (int i = 0; i < rpad; i++) *p++ = ' ';
    *p++ = '|';
    *p++ = '\n';
    /* bottom border */
    *p++ = '+';
    for (int i = 0; i < width - 2; i++) *p++ = '-';
    *p++ = '+';
    *p = '\0';
    return result;
}

/* ---- strip ANSI codes from string ---- */
const char *aria_cli_strip(const char *text) {
    char *out = result;
    const char *p = text;
    while (*p && (out - result) < MAX_BUF - 1) {
        if (*p == '\033' && *(p+1) == '[') {
            p += 2;
            while (*p && *p != 'm') p++;
            if (*p == 'm') p++;
        } else {
            *out++ = *p++;
        }
    }
    *out = '\0';
    return result;
}
