/* aria_semver_shim.c — Semantic versioning parser */
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static int32_t sv_major, sv_minor, sv_patch;
static char sv_prerelease[128];
static char sv_build[128];
static char sv_string[256];

/* ---- parse semver string ---- */
int32_t aria_semver_parse(const char *version) {
    sv_major = sv_minor = sv_patch = 0;
    sv_prerelease[0] = sv_build[0] = '\0';
    if (!version || !*version) return 0;

    const char *p = version;
    if (*p == 'v' || *p == 'V') p++;

    /* major */
    char *end;
    long val = strtol(p, &end, 10);
    if (end == p) return 0;
    sv_major = (int32_t)val;
    p = end;
    if (*p != '.') return 1;
    p++;

    /* minor */
    val = strtol(p, &end, 10);
    if (end == p) return 0;
    sv_minor = (int32_t)val;
    p = end;
    if (*p != '.') return 1;
    p++;

    /* patch */
    val = strtol(p, &end, 10);
    if (end == p) return 0;
    sv_patch = (int32_t)val;
    p = end;

    /* prerelease */
    if (*p == '-') {
        p++;
        const char *plus = strchr(p, '+');
        size_t len = plus ? (size_t)(plus - p) : strlen(p);
        if (len >= sizeof(sv_prerelease)) len = sizeof(sv_prerelease) - 1;
        memcpy(sv_prerelease, p, len);
        sv_prerelease[len] = '\0';
        p += len;
    }

    /* build */
    if (*p == '+') {
        p++;
        strncpy(sv_build, p, sizeof(sv_build) - 1);
        sv_build[sizeof(sv_build) - 1] = '\0';
    }

    return 1;
}

int32_t aria_semver_get_major(void)      { return sv_major; }
int32_t aria_semver_get_minor(void)      { return sv_minor; }
int32_t aria_semver_get_patch(void)      { return sv_patch; }
const char *aria_semver_get_prerelease(void) { return sv_prerelease; }
const char *aria_semver_get_build(void)  { return sv_build; }

/* ---- format to string ---- */
const char *aria_semver_to_string(void) {
    if (sv_prerelease[0] && sv_build[0])
        snprintf(sv_string, sizeof(sv_string), "%d.%d.%d-%s+%s", sv_major, sv_minor, sv_patch, sv_prerelease, sv_build);
    else if (sv_prerelease[0])
        snprintf(sv_string, sizeof(sv_string), "%d.%d.%d-%s", sv_major, sv_minor, sv_patch, sv_prerelease);
    else if (sv_build[0])
        snprintf(sv_string, sizeof(sv_string), "%d.%d.%d+%s", sv_major, sv_minor, sv_patch, sv_build);
    else
        snprintf(sv_string, sizeof(sv_string), "%d.%d.%d", sv_major, sv_minor, sv_patch);
    return sv_string;
}

/* ---- compare two version strings ----
   Returns: -1 if a < b, 0 if equal, 1 if a > b */
int32_t aria_semver_compare(const char *a, const char *b) {
    int32_t am, ami, ap, bm, bmi, bp;

    /* Parse a */
    const char *p = a;
    if (*p == 'v' || *p == 'V') p++;
    am = (int32_t)strtol(p, (char**)&p, 10); if (*p == '.') p++;
    ami = (int32_t)strtol(p, (char**)&p, 10); if (*p == '.') p++;
    ap = (int32_t)strtol(p, (char**)&p, 10);

    /* Parse b */
    p = b;
    if (*p == 'v' || *p == 'V') p++;
    bm = (int32_t)strtol(p, (char**)&p, 10); if (*p == '.') p++;
    bmi = (int32_t)strtol(p, (char**)&p, 10); if (*p == '.') p++;
    bp = (int32_t)strtol(p, (char**)&p, 10);

    if (am != bm) return am < bm ? -1 : 1;
    if (ami != bmi) return ami < bmi ? -1 : 1;
    if (ap != bp) return ap < bp ? -1 : 1;
    return 0;
}

/* ---- check if version satisfies >=min constraint ---- */
int32_t aria_semver_satisfies(const char *version, const char *constraint) {
    /* Simple >=X.Y.Z check */
    const char *c = constraint;
    if (*c == '>' && *(c+1) == '=') c += 2;
    else if (*c == '>') c += 1;
    int32_t cmp = aria_semver_compare(version, c);
    return cmp >= 0 ? 1 : 0;
}

/* ---- bump major/minor/patch ---- */
const char *aria_semver_bump_major(const char *version) {
    aria_semver_parse(version);
    sv_major++;
    sv_minor = 0;
    sv_patch = 0;
    sv_prerelease[0] = '\0';
    sv_build[0] = '\0';
    return aria_semver_to_string();
}

const char *aria_semver_bump_minor(const char *version) {
    aria_semver_parse(version);
    sv_minor++;
    sv_patch = 0;
    sv_prerelease[0] = '\0';
    sv_build[0] = '\0';
    return aria_semver_to_string();
}

const char *aria_semver_bump_patch(const char *version) {
    aria_semver_parse(version);
    sv_patch++;
    sv_prerelease[0] = '\0';
    sv_build[0] = '\0';
    return aria_semver_to_string();
}
