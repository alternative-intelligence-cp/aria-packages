/* aria_mime_shim.c — MIME type detection from file extensions */
#include <stdint.h>
#include <string.h>
#include <ctype.h>

typedef struct { const char *ext; const char *mime; } mime_entry_t;

static const mime_entry_t mime_table[] = {
    /* text */
    {".html",  "text/html"},
    {".htm",   "text/html"},
    {".css",   "text/css"},
    {".js",    "text/javascript"},
    {".mjs",   "text/javascript"},
    {".json",  "application/json"},
    {".xml",   "application/xml"},
    {".txt",   "text/plain"},
    {".csv",   "text/csv"},
    {".md",    "text/markdown"},
    {".yaml",  "text/yaml"},
    {".yml",   "text/yaml"},
    {".toml",  "application/toml"},
    {".ini",   "text/plain"},
    {".cfg",   "text/plain"},
    {".log",   "text/plain"},
    /* images */
    {".png",   "image/png"},
    {".jpg",   "image/jpeg"},
    {".jpeg",  "image/jpeg"},
    {".gif",   "image/gif"},
    {".bmp",   "image/bmp"},
    {".svg",   "image/svg+xml"},
    {".webp",  "image/webp"},
    {".ico",   "image/x-icon"},
    {".tiff",  "image/tiff"},
    {".tif",   "image/tiff"},
    {".avif",  "image/avif"},
    /* audio */
    {".mp3",   "audio/mpeg"},
    {".wav",   "audio/wav"},
    {".ogg",   "audio/ogg"},
    {".flac",  "audio/flac"},
    {".aac",   "audio/aac"},
    {".m4a",   "audio/mp4"},
    {".wma",   "audio/x-ms-wma"},
    /* video */
    {".mp4",   "video/mp4"},
    {".webm",  "video/webm"},
    {".avi",   "video/x-msvideo"},
    {".mov",   "video/quicktime"},
    {".mkv",   "video/x-matroska"},
    {".flv",   "video/x-flv"},
    {".wmv",   "video/x-ms-wmv"},
    /* application */
    {".pdf",   "application/pdf"},
    {".zip",   "application/zip"},
    {".gz",    "application/gzip"},
    {".tar",   "application/x-tar"},
    {".7z",    "application/x-7z-compressed"},
    {".rar",   "application/vnd.rar"},
    {".bz2",   "application/x-bzip2"},
    {".xz",    "application/x-xz"},
    {".wasm",  "application/wasm"},
    {".doc",   "application/msword"},
    {".docx",  "application/vnd.openxmlformats-officedocument.wordprocessingml.document"},
    {".xls",   "application/vnd.ms-excel"},
    {".xlsx",  "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"},
    {".ppt",   "application/vnd.ms-powerpoint"},
    {".pptx",  "application/vnd.openxmlformats-officedocument.presentationml.presentation"},
    /* fonts */
    {".woff",  "font/woff"},
    {".woff2", "font/woff2"},
    {".ttf",   "font/ttf"},
    {".otf",   "font/otf"},
    {".eot",   "application/vnd.ms-fontobject"},
    /* programming */
    {".c",     "text/x-c"},
    {".h",     "text/x-c"},
    {".cpp",   "text/x-c++"},
    {".py",    "text/x-python"},
    {".java",  "text/x-java"},
    {".rs",    "text/x-rust"},
    {".go",    "text/x-go"},
    {".ts",    "text/typescript"},
    {".sh",    "application/x-sh"},
    {".aria",  "text/x-aria"},
    {NULL, NULL}
};

#define MAX_MIME 256
static char result[MAX_MIME];
static char ext_buf[64];

/* Get lowercase extension from filename */
static const char *get_ext(const char *filename) {
    const char *dot = NULL;
    for (const char *p = filename; *p; p++)
        if (*p == '.') dot = p;
    if (!dot) return NULL;

    size_t i = 0;
    for (const char *p = dot; *p && i < sizeof(ext_buf) - 1; p++, i++)
        ext_buf[i] = (char)tolower((unsigned char)*p);
    ext_buf[i] = '\0';
    return ext_buf;
}

/* ---- lookup MIME type by filename ---- */
const char *aria_mime_from_filename(const char *filename) {
    const char *ext = get_ext(filename);
    if (!ext) return "application/octet-stream";

    for (const mime_entry_t *m = mime_table; m->ext; m++) {
        if (strcmp(ext, m->ext) == 0) return m->mime;
    }
    return "application/octet-stream";
}

/* ---- lookup MIME type by extension (with or without dot) ---- */
const char *aria_mime_from_extension(const char *ext) {
    if (!ext || !*ext) return "application/octet-stream";

    /* Normalize: add dot if missing, lowercase */
    size_t i = 0;
    if (ext[0] != '.') ext_buf[i++] = '.';
    for (const char *p = ext; *p && i < sizeof(ext_buf) - 1; p++, i++)
        ext_buf[i] = (char)tolower((unsigned char)*p);
    ext_buf[i] = '\0';

    for (const mime_entry_t *m = mime_table; m->ext; m++) {
        if (strcmp(ext_buf, m->ext) == 0) return m->mime;
    }
    return "application/octet-stream";
}

/* ---- get extension from MIME type ---- */
const char *aria_mime_to_extension(const char *mime) {
    if (!mime || !*mime) return "";
    /* lowercase compare */
    char lower[MAX_MIME];
    size_t i;
    for (i = 0; mime[i] && i < sizeof(lower) - 1; i++)
        lower[i] = (char)tolower((unsigned char)mime[i]);
    lower[i] = '\0';

    for (const mime_entry_t *m = mime_table; m->ext; m++) {
        if (strcmp(lower, m->mime) == 0) return m->ext;
    }
    return "";
}

/* ---- check if MIME type is text ---- */
int32_t aria_mime_is_text(const char *mime) {
    if (!mime) return 0;
    if (strncmp(mime, "text/", 5) == 0) return 1;
    if (strcmp(mime, "application/json") == 0) return 1;
    if (strcmp(mime, "application/xml") == 0) return 1;
    if (strcmp(mime, "application/toml") == 0) return 1;
    if (strcmp(mime, "application/x-sh") == 0) return 1;
    return 0;
}

/* ---- count registered types ---- */
int32_t aria_mime_count(void) {
    int32_t n = 0;
    for (const mime_entry_t *m = mime_table; m->ext; m++) n++;
    return n;
}
