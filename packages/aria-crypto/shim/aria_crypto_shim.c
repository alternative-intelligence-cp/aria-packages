/* aria_crypto_shim.c — Cryptographic hashing (self-contained, no OpenSSL) */
#include <stdint.h>
#include <string.h>
#include <stdio.h>

/* ========== MD5 Implementation ========== */

static void md5_transform(uint32_t state[4], const uint8_t block[64]) {
    static const uint32_t K[64] = {
        0xd76aa478,0xe8c7b756,0x242070db,0xc1bdceee,0xf57c0faf,0x4787c62a,0xa8304613,0xfd469501,
        0x698098d8,0x8b44f7af,0xffff5bb1,0x895cd7be,0x6b901122,0xfd987193,0xa679438e,0x49b40821,
        0xf61e2562,0xc040b340,0x265e5a51,0xe9b6c7aa,0xd62f105d,0x02441453,0xd8a1e681,0xe7d3fbc8,
        0x21e1cde6,0xc33707d6,0xf4d50d87,0x455a14ed,0xa9e3e905,0xfcefa3f8,0x676f02d9,0x8d2a4c8a,
        0xfffa3942,0x8771f681,0x6d9d6122,0xfde5380c,0xa4beea44,0x4bdecfa9,0xf6bb4b60,0xbebfbc70,
        0x289b7ec6,0xeaa127fa,0xd4ef3085,0x04881d05,0xd9d4d039,0xe6db99e5,0x1fa27cf8,0xc4ac5665,
        0xf4292244,0x432aff97,0xab9423a7,0xfc93a039,0x655b59c3,0x8f0ccc92,0xffeff47d,0x85845dd1,
        0x6fa87e4f,0xfe2ce6e0,0xa3014314,0x4e0811a1,0xf7537e82,0xbd3af235,0x2ad7d2bb,0xeb86d391
    };
    static const uint8_t S[64] = {
        7,12,17,22,7,12,17,22,7,12,17,22,7,12,17,22,
        5,9,14,20,5,9,14,20,5,9,14,20,5,9,14,20,
        4,11,16,23,4,11,16,23,4,11,16,23,4,11,16,23,
        6,10,15,21,6,10,15,21,6,10,15,21,6,10,15,21
    };
    uint32_t M[16];
    for (int i = 0; i < 16; i++)
        M[i] = (uint32_t)block[i*4] | ((uint32_t)block[i*4+1]<<8) | ((uint32_t)block[i*4+2]<<16) | ((uint32_t)block[i*4+3]<<24);

    uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
    for (int i = 0; i < 64; i++) {
        uint32_t f, g;
        if (i < 16)      { f = (b & c) | (~b & d); g = i; }
        else if (i < 32) { f = (d & b) | (~d & c); g = (5*i+1)%16; }
        else if (i < 48) { f = b ^ c ^ d;          g = (3*i+5)%16; }
        else              { f = c ^ (b | ~d);       g = (7*i)%16; }
        uint32_t tmp = d; d = c; c = b;
        uint32_t x = a + f + K[i] + M[g];
        b = b + ((x << S[i]) | (x >> (32 - S[i])));
        a = tmp;
    }
    state[0] += a; state[1] += b; state[2] += c; state[3] += d;
}

static void md5_hash(const uint8_t *data, size_t len, uint8_t out[16]) {
    uint32_t state[4] = { 0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476 };
    size_t i;
    for (i = 0; i + 64 <= len; i += 64)
        md5_transform(state, data + i);
    uint8_t buf[128];
    size_t rem = len - i;
    memcpy(buf, data + i, rem);
    buf[rem++] = 0x80;
    if (rem > 56) {
        memset(buf + rem, 0, 64 - rem);
        md5_transform(state, buf);
        rem = 0;
    }
    memset(buf + rem, 0, 56 - rem);
    uint64_t bits = (uint64_t)len * 8;
    for (int j = 0; j < 8; j++) buf[56 + j] = (uint8_t)(bits >> (j * 8));
    md5_transform(state, buf);
    for (int j = 0; j < 4; j++) {
        out[j*4]   = (uint8_t)(state[j]);
        out[j*4+1] = (uint8_t)(state[j]>>8);
        out[j*4+2] = (uint8_t)(state[j]>>16);
        out[j*4+3] = (uint8_t)(state[j]>>24);
    }
}

/* ========== SHA-256 Implementation ========== */

static const uint32_t sha256_k[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

#define RR(x,n) (((x)>>(n))|((x)<<(32-(n))))
#define CH(x,y,z) (((x)&(y))^(~(x)&(z)))
#define MAJ(x,y,z) (((x)&(y))^((x)&(z))^((y)&(z)))
#define EP0(x) (RR(x,2)^RR(x,13)^RR(x,22))
#define EP1(x) (RR(x,6)^RR(x,11)^RR(x,25))
#define SIG0(x) (RR(x,7)^RR(x,18)^((x)>>3))
#define SIG1(x) (RR(x,17)^RR(x,19)^((x)>>10))

static void sha256_transform(uint32_t state[8], const uint8_t block[64]) {
    uint32_t w[64];
    for (int i = 0; i < 16; i++)
        w[i] = ((uint32_t)block[i*4]<<24) | ((uint32_t)block[i*4+1]<<16) | ((uint32_t)block[i*4+2]<<8) | block[i*4+3];
    for (int i = 16; i < 64; i++)
        w[i] = SIG1(w[i-2]) + w[i-7] + SIG0(w[i-15]) + w[i-16];

    uint32_t a=state[0],b=state[1],c=state[2],d=state[3],e=state[4],f=state[5],g=state[6],h=state[7];
    for (int i = 0; i < 64; i++) {
        uint32_t t1 = h + EP1(e) + CH(e,f,g) + sha256_k[i] + w[i];
        uint32_t t2 = EP0(a) + MAJ(a,b,c);
        h=g; g=f; f=e; e=d+t1; d=c; c=b; b=a; a=t1+t2;
    }
    state[0]+=a; state[1]+=b; state[2]+=c; state[3]+=d;
    state[4]+=e; state[5]+=f; state[6]+=g; state[7]+=h;
}

static void sha256_hash(const uint8_t *data, size_t len, uint8_t out[32]) {
    uint32_t state[8] = {
        0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,
        0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19
    };
    size_t i;
    for (i = 0; i + 64 <= len; i += 64)
        sha256_transform(state, data + i);
    uint8_t buf[128];
    size_t rem = len - i;
    memcpy(buf, data + i, rem);
    buf[rem++] = 0x80;
    if (rem > 56) {
        memset(buf + rem, 0, 64 - rem);
        sha256_transform(state, buf);
        rem = 0;
    }
    memset(buf + rem, 0, 56 - rem);
    uint64_t bits = (uint64_t)len * 8;
    for (int j = 7; j >= 0; j--) buf[56 + (7 - j)] = (uint8_t)(bits >> (j * 8));
    sha256_transform(state, buf);
    for (int j = 0; j < 8; j++) {
        out[j*4]   = (uint8_t)(state[j]>>24);
        out[j*4+1] = (uint8_t)(state[j]>>16);
        out[j*4+2] = (uint8_t)(state[j]>>8);
        out[j*4+3] = (uint8_t)(state[j]);
    }
}

/* ========== HMAC-SHA256 ========== */

static void hmac_sha256(const uint8_t *key, size_t klen, const uint8_t *msg, size_t mlen, uint8_t out[32]) {
    uint8_t kpad[64];
    if (klen > 64) {
        sha256_hash(key, klen, kpad);
        klen = 32;
        memset(kpad + 32, 0, 32);
    } else {
        memcpy(kpad, key, klen);
        memset(kpad + klen, 0, 64 - klen);
    }

    uint8_t ipad[64 + 4096];
    uint8_t opad[64 + 32];
    for (int i = 0; i < 64; i++) {
        ipad[i] = kpad[i] ^ 0x36;
        opad[i] = kpad[i] ^ 0x5c;
    }
    memcpy(ipad + 64, msg, mlen);
    sha256_hash(ipad, 64 + mlen, opad + 64);
    sha256_hash(opad, 64 + 32, out);
}

/* ========== Hex encoding ========== */

static char hex_result[129];

static const char *to_hex(const uint8_t *data, size_t len) {
    static const char hex[] = "0123456789abcdef";
    size_t limit = len;
    if (limit > 64) limit = 64;
    for (size_t i = 0; i < limit; i++) {
        hex_result[i*2]   = hex[(data[i] >> 4) & 0xf];
        hex_result[i*2+1] = hex[data[i] & 0xf];
    }
    hex_result[limit*2] = '\0';
    return hex_result;
}

/* ========== Public API ========== */

const char *aria_crypto_sha256(const char *data) {
    uint8_t hash[32];
    sha256_hash((const uint8_t *)data, strlen(data), hash);
    return to_hex(hash, 32);
}

const char *aria_crypto_md5(const char *data) {
    uint8_t hash[16];
    md5_hash((const uint8_t *)data, strlen(data), hash);
    return to_hex(hash, 16);
}

const char *aria_crypto_hmac_sha256(const char *key, const char *data) {
    uint8_t hash[32];
    hmac_sha256((const uint8_t *)key, strlen(key), (const uint8_t *)data, strlen(data), hash);
    return to_hex(hash, 32);
}

int32_t aria_crypto_sha256_verify(const char *data, const char *expected_hex) {
    const char *computed = aria_crypto_sha256(data);
    return (strcmp(computed, expected_hex) == 0) ? 1 : 0;
}

int32_t aria_crypto_md5_verify(const char *data, const char *expected_hex) {
    const char *computed = aria_crypto_md5(data);
    return (strcmp(computed, expected_hex) == 0) ? 1 : 0;
}
