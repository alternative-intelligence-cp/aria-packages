/* aria_xml_shim.c — Simple XML parser (self-contained, no external deps) */
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#define MAX_NODES    512
#define MAX_TAG      128
#define MAX_TEXT     4096
#define MAX_ATTRS    16
#define MAX_ATTR_KEY 64
#define MAX_ATTR_VAL 256
#define MAX_PATH     512

typedef struct {
    char key[MAX_ATTR_KEY];
    char val[MAX_ATTR_VAL];
} xml_attr_t;

typedef struct {
    char tag[MAX_TAG];
    char text[MAX_TEXT];
    int32_t parent;       /* -1 for root */
    int32_t depth;
    xml_attr_t attrs[MAX_ATTRS];
    int attr_count;
} xml_node_t;

static xml_node_t nodes[MAX_NODES];
static int node_count = 0;
static char query_result[MAX_TEXT];

/* ---- skip whitespace ---- */
static const char *skip_ws(const char *p) {
    while (*p && isspace((unsigned char)*p)) p++;
    return p;
}

/* ---- parse attributes within a tag ---- */
static int parse_attrs(const char *p, const char *end, xml_node_t *node) {
    node->attr_count = 0;
    while (p < end && node->attr_count < MAX_ATTRS) {
        p = skip_ws(p);
        if (p >= end) break;

        /* key */
        const char *ks = p;
        while (p < end && *p != '=' && !isspace((unsigned char)*p)) p++;
        if (p == ks) break;
        size_t klen = (size_t)(p - ks);
        if (klen >= MAX_ATTR_KEY) klen = MAX_ATTR_KEY - 1;
        memcpy(node->attrs[node->attr_count].key, ks, klen);
        node->attrs[node->attr_count].key[klen] = '\0';

        p = skip_ws(p);
        if (*p != '=') break;
        p++;
        p = skip_ws(p);

        /* value */
        char quote = *p;
        if (quote != '"' && quote != '\'') break;
        p++;
        const char *vs = p;
        while (p < end && *p != quote) p++;
        size_t vlen = (size_t)(p - vs);
        if (vlen >= MAX_ATTR_VAL) vlen = MAX_ATTR_VAL - 1;
        memcpy(node->attrs[node->attr_count].val, vs, vlen);
        node->attrs[node->attr_count].val[vlen] = '\0';
        if (*p == quote) p++;
        node->attr_count++;
    }
    return node->attr_count;
}

/* ---- parse XML string ---- */
int32_t aria_xml_parse(const char *xml) {
    node_count = 0;
    if (!xml || !*xml) return 0;

    const char *p = xml;
    int32_t parent_stack[64];
    int stack_depth = 0;
    memset(parent_stack, -1, sizeof(parent_stack));

    while (*p && node_count < MAX_NODES) {
        p = skip_ws(p);
        if (!*p) break;

        if (*p == '<') {
            p++;
            /* Skip XML declaration and comments */
            if (*p == '?') { p = strstr(p, "?>"); if (p) p += 2; else break; continue; }
            if (*p == '!') { p = strstr(p, ">"); if (p) p++; else break; continue; }

            /* Closing tag */
            if (*p == '/') {
                p++;
                while (*p && *p != '>') p++;
                if (*p == '>') p++;
                if (stack_depth > 0) stack_depth--;
                continue;
            }

            /* Opening tag */
            const char *tag_start = p;
            while (*p && *p != '>' && *p != ' ' && *p != '/' && *p != '\t' && *p != '\n') p++;
            size_t tlen = (size_t)(p - tag_start);
            if (tlen == 0) continue;
            if (tlen >= MAX_TAG) tlen = MAX_TAG - 1;

            int idx = node_count;
            memcpy(nodes[idx].tag, tag_start, tlen);
            nodes[idx].tag[tlen] = '\0';
            nodes[idx].text[0] = '\0';
            nodes[idx].parent = stack_depth > 0 ? parent_stack[stack_depth - 1] : -1;
            nodes[idx].depth = stack_depth;
            nodes[idx].attr_count = 0;

            /* Parse attributes */
            const char *attr_start = p;
            int self_closing = 0;
            while (*p && *p != '>') {
                if (*p == '/' && *(p+1) == '>') { self_closing = 1; break; }
                p++;
            }
            if (attr_start < p) {
                const char *attr_end = self_closing ? p : p;
                parse_attrs(attr_start, attr_end, &nodes[idx]);
            }
            if (self_closing) { p += 2; }
            else {
                if (*p == '>') p++;

                /* Read text content */
                const char *text_start = p;
                while (*p && *p != '<') p++;
                size_t text_len = (size_t)(p - text_start);
                /* Trim whitespace */
                while (text_len > 0 && isspace((unsigned char)text_start[text_len-1])) text_len--;
                while (text_len > 0 && isspace((unsigned char)*text_start)) { text_start++; text_len--; }
                if (text_len >= MAX_TEXT) text_len = MAX_TEXT - 1;
                memcpy(nodes[idx].text, text_start, text_len);
                nodes[idx].text[text_len] = '\0';

                /* Push to stack */
                if (stack_depth < 63) {
                    parent_stack[stack_depth] = idx;
                    stack_depth++;
                }
            }
            node_count++;
        } else {
            p++;
        }
    }

    return (int32_t)node_count;
}

/* ---- get text of first element with given tag ---- */
const char *aria_xml_get_text(const char *tag) {
    for (int i = 0; i < node_count; i++) {
        if (strcmp(nodes[i].tag, tag) == 0) return nodes[i].text;
    }
    return "";
}

/* ---- get attribute of first element with given tag ---- */
const char *aria_xml_get_attr(const char *tag, const char *attr_name) {
    for (int i = 0; i < node_count; i++) {
        if (strcmp(nodes[i].tag, tag) == 0) {
            for (int j = 0; j < nodes[i].attr_count; j++) {
                if (strcmp(nodes[i].attrs[j].key, attr_name) == 0)
                    return nodes[i].attrs[j].val;
            }
            return "";
        }
    }
    return "";
}

/* ---- count elements with given tag ---- */
int32_t aria_xml_count_tag(const char *tag) {
    int32_t n = 0;
    for (int i = 0; i < node_count; i++) {
        if (strcmp(nodes[i].tag, tag) == 0) n++;
    }
    return n;
}

/* ---- has element with tag ---- */
int32_t aria_xml_has_tag(const char *tag) {
    for (int i = 0; i < node_count; i++) {
        if (strcmp(nodes[i].tag, tag) == 0) return 1;
    }
    return 0;
}

/* ---- total node count ---- */
int32_t aria_xml_node_count(void) {
    return (int32_t)node_count;
}

/* ---- clear ---- */
void aria_xml_clear(void) {
    node_count = 0;
}

/* ---- get text by path e.g. "root/child/item" ---- */
const char *aria_xml_get_path(const char *path) {
    /* Split path by / and walk tree */
    char parts[8][MAX_TAG];
    int nparts = 0;
    const char *p = path;
    while (*p && nparts < 8) {
        const char *s = p;
        while (*p && *p != '/') p++;
        size_t len = (size_t)(p - s);
        if (len >= MAX_TAG) len = MAX_TAG - 1;
        memcpy(parts[nparts], s, len);
        parts[nparts][len] = '\0';
        nparts++;
        if (*p == '/') p++;
    }
    if (nparts == 0) return "";

    /* Find matching node: last part with correct ancestry */
    for (int i = 0; i < node_count; i++) {
        if (strcmp(nodes[i].tag, parts[nparts - 1]) != 0) continue;
        if (nodes[i].depth != nparts - 1) continue;

        /* Verify ancestry */
        int match = 1;
        int ni = i;
        for (int d = nparts - 2; d >= 0; d--) {
            int pi = nodes[ni].parent;
            if (pi < 0 || strcmp(nodes[pi].tag, parts[d]) != 0) { match = 0; break; }
            ni = pi;
        }
        if (match) return nodes[i].text;
    }
    return "";
}
