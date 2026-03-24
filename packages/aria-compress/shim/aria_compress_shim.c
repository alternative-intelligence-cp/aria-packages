/* aria_compress_shim.c — gzip/deflate compression via zlib */
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

/* Internal buffers for compressed/decompressed results */
#define MAX_BUF (4 * 1024 * 1024)  /* 4 MB max */

static unsigned char out_buf[MAX_BUF];
static unsigned long out_len = 0;
static int last_error = 0;

/* ---- compress (deflate, raw) ---- */
int32_t aria_compress_deflate(const char *data, int32_t data_len) {
    out_len = MAX_BUF;
    int ret = compress2(out_buf, &out_len, (const Bytef *)data, (uLong)data_len, Z_DEFAULT_COMPRESSION);
    last_error = ret;
    if (ret != Z_OK) return -1;
    return (int32_t)out_len;
}

/* ---- decompress (inflate, raw) ---- */
int32_t aria_compress_inflate(const char *data, int32_t data_len) {
    out_len = MAX_BUF;
    int ret = uncompress(out_buf, &out_len, (const Bytef *)data, (uLong)data_len);
    last_error = ret;
    if (ret != Z_OK) return -1;
    return (int32_t)out_len;
}

/* ---- gzip compress ---- */
int32_t aria_compress_gzip(const char *data, int32_t data_len) {
    z_stream strm;
    memset(&strm, 0, sizeof(strm));
    /* windowBits = 15 + 16 for gzip header */
    int ret = deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY);
    if (ret != Z_OK) { last_error = ret; return -1; }

    strm.next_in  = (Bytef *)data;
    strm.avail_in = (uInt)data_len;
    strm.next_out = out_buf;
    strm.avail_out = MAX_BUF;

    ret = deflate(&strm, Z_FINISH);
    if (ret != Z_STREAM_END) {
        deflateEnd(&strm);
        last_error = ret;
        return -1;
    }
    out_len = strm.total_out;
    deflateEnd(&strm);
    last_error = 0;
    return (int32_t)out_len;
}

/* ---- gzip decompress ---- */
int32_t aria_compress_gunzip(const char *data, int32_t data_len) {
    z_stream strm;
    memset(&strm, 0, sizeof(strm));
    /* windowBits = 15 + 16 for gzip */
    int ret = inflateInit2(&strm, 15 + 16);
    if (ret != Z_OK) { last_error = ret; return -1; }

    strm.next_in  = (Bytef *)data;
    strm.avail_in = (uInt)data_len;
    strm.next_out = out_buf;
    strm.avail_out = MAX_BUF;

    ret = inflate(&strm, Z_FINISH);
    if (ret != Z_STREAM_END) {
        inflateEnd(&strm);
        last_error = ret;
        return -1;
    }
    out_len = strm.total_out;
    inflateEnd(&strm);
    last_error = 0;
    return (int32_t)out_len;
}

/* ---- get result pointer (as string) ---- */
const char *aria_compress_get_result(void) {
    /* Null-terminate for safety when result is text */
    if (out_len < MAX_BUF) out_buf[out_len] = '\0';
    return (const char *)out_buf;
}

/* ---- get result length ---- */
int32_t aria_compress_get_length(void) {
    return (int32_t)out_len;
}

/* ---- get last error code ---- */
int32_t aria_compress_get_error(void) {
    return (int32_t)last_error;
}

/* ---- compress string and return string (for text round-trip testing) ---- */
/* Convenience: deflate a string, then inflate it back, store result */
int32_t aria_compress_roundtrip(const char *data, int32_t data_len) {
    /* First compress */
    unsigned long comp_len = MAX_BUF;
    unsigned char comp_buf[MAX_BUF];
    int ret = compress2(comp_buf, &comp_len, (const Bytef *)data, (uLong)data_len, Z_DEFAULT_COMPRESSION);
    if (ret != Z_OK) { last_error = ret; return -1; }

    /* Then decompress */
    out_len = MAX_BUF;
    ret = uncompress(out_buf, &out_len, comp_buf, comp_len);
    if (ret != Z_OK) { last_error = ret; return -1; }
    out_buf[out_len] = '\0';
    last_error = 0;
    return (int32_t)out_len;
}

/* ---- get zlib version ---- */
const char *aria_compress_version(void) {
    return zlibVersion();
}
