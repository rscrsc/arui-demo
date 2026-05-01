// SPDX-License-Identifier: 0BSD
// Copyright (c) 2026 Arcie Ren

/* jr.h — JSON token scanner. C99. Single header. Zero alloc. Zero deps.
 *
 * Does one thing: splits a JSON string into tokens, one at a time.
 * You decide what to keep and what to skip. Like strtok for JSON.
 *
 * ================================================================
 *  Token types (ASCII chars — readable in printf)
 * ================================================================
 *
 *   {  }  [  ]  :  ,       structural
 *   "                      string   (tok/len exclude quotes)
 *   #                      number
 *   t  f  n                true / false / null
 *   0                      end of input
 *  -1                      error
 *
 * ================================================================
 *  Low-level: raw token scan
 * ================================================================
 *
 *   jr r = jr_new(text);
 *   while (jr_step(&r))
 *       printf("%c  '%.*s'\n", r.type, r.len, r.tok);
 *
 * ================================================================
 *  Parse a known action message
 * ================================================================
 *
 *   // {"type":"run","cmd":"ls -la","index":5}
 *
 *   char type[32] = {0}, cmd[256] = {0};
 *   int  index = -1;
 *
 *   jr r = jr_new(buf);
 *   jr_open(&r);
 *   while (jr_key(&r)) {
 *       if      (jr_eq(&r, "type"))  jr_val_str(&r, type, sizeof type);
 *       else if (jr_eq(&r, "cmd"))   jr_val_str(&r, cmd,  sizeof cmd);
 *       else if (jr_eq(&r, "index")) index = jr_val_int(&r);
 *       else                         jr_val_skip(&r);
 *   }
 *
 * ================================================================
 *  Read an array of numbers
 * ================================================================
 *
 *   // {"data":[1.5,2.3,3.7],"count":3}
 *
 *   double data[1024]; int dn = 0, count = 0;
 *
 *   jr r = jr_new(buf);
 *   jr_open(&r);
 *   while (jr_key(&r)) {
 *       if (jr_eq(&r, "data"))
 *           dn = jr_val_doubles(&r, data, 1024);
 *       else if (jr_eq(&r, "count"))
 *           count = jr_val_int(&r);
 *       else
 *           jr_val_skip(&r);
 *   }
 *
 * ================================================================
 *  Read an array of numbers — manual loop
 * ================================================================
 *
 *   // [10, 20, 30]
 *
 *   int vals[64], n = 0;
 *   jr r = jr_new(buf);
 *   jr_open(&r);
 *   while (r.type == '#') {
 *       vals[n++] = jr_as_int(&r);
 *       jr_step(&r);
 *       if (r.type == ',') jr_step(&r);
 *   }
 *   jr_close(&r);
 *
 * ================================================================
 *  Nested objects
 * ================================================================
 *
 *   // {"pt":{"x":1,"y":2},"label":"A"}
 *
 *   int x = 0, y = 0;
 *   char label[32] = {0};
 *
 *   jr r = jr_new(buf);
 *   jr_open(&r);
 *   while (jr_key(&r)) {
 *       if (jr_eq(&r, "pt")) {
 *           jr_open(&r);
 *           while (jr_key(&r)) {
 *               if      (jr_eq(&r, "x")) x = jr_val_int(&r);
 *               else if (jr_eq(&r, "y")) y = jr_val_int(&r);
 *               else jr_val_skip(&r);
 *           }
 *       }
 *       else if (jr_eq(&r, "label")) jr_val_str(&r, label, sizeof label);
 *       else jr_val_skip(&r);
 *   }
 *
 * ================================================================
 *  Array of objects
 * ================================================================
 *
 *   // {"items":[{"id":1,"s":"a"},{"id":2,"s":"b"}]}
 *
 *   jr r = jr_new(buf);
 *   jr_open(&r);
 *   while (jr_key(&r)) {
 *       if (jr_eq(&r, "items")) {
 *           jr_open(&r);                  // enter [
 *           while (r.type == '{') {
 *               int id = 0; char s[32] = {0};
 *               jr_open(&r);              // enter {
 *               while (jr_key(&r)) {
 *                   if      (jr_eq(&r,"id")) id = jr_val_int(&r);
 *                   else if (jr_eq(&r,"s"))  jr_val_str(&r, s, sizeof s);
 *                   else jr_val_skip(&r);
 *               }
 *               // jr_key returned 0 → r.type is '}'
 *               // step past }
 *               jr_step(&r);
 *               if (r.type == ',') jr_step(&r);
 *               // use id, s ...
 *           }
 *           jr_close(&r);                 // step past ]
 *       }
 *       else jr_val_skip(&r);
 *   }
 *
 * ================================================================
 *  jr_val_skip handles anything you don't care about:
 *  numbers, strings, booleans, nulls, nested objects, nested arrays.
 * ================================================================
 */

#ifndef JR_H
#define JR_H

#include <string.h>
#include <stdlib.h>

/* ================================================================
 *  Scanner state
 * ================================================================ */

typedef struct {
    const char *cur;                  /* read head                      */
    const char *tok;  int len;        /* current token span             */
    int         type;                 /* token type (see enum above)    */
    const char *kp;   int kn;         /* last key (set by jr_key)       */
} jr;

static inline jr jr_new(const char *s) {
    jr r;
    memset(&r, 0, sizeof r);
    r.cur = s ? s : "";
    return r;
}

/* ================================================================
 *  Core: advance to next token. Returns type, 0 on end.
 * ================================================================ */

static inline int jr_step(jr *r) {
    while (*r->cur == ' '  || *r->cur == '\t' ||
           *r->cur == '\n' || *r->cur == '\r') r->cur++;

    const char *p = r->cur;
    if (!*p) { r->type = 0; r->tok = p; r->len = 0; return 0; }

    switch (*p) {
    case '{': case '}': case '[': case ']': case ':': case ',':
        r->type = *p; r->tok = p; r->len = 1; r->cur = p + 1;
        return r->type;

    case '"':
        p++; r->tok = p;
        while (*p && *p != '"') { if (*p == '\\') p++; p++; }
        r->len = (int)(p - r->tok);
        r->type = '"';
        if (*p == '"') p++;
        r->cur = p;
        return '"';

    case 't': r->tok=p; r->len=4; r->cur=p+4; r->type='t'; return 't';
    case 'f': r->tok=p; r->len=5; r->cur=p+5; r->type='f'; return 'f';
    case 'n': r->tok=p; r->len=4; r->cur=p+4; r->type='n'; return 'n';

    default:
        if (*p == '-' || (*p >= '0' && *p <= '9')) {
            r->tok = p;
            if (*p == '-') p++;
            while (*p >= '0' && *p <= '9') p++;
            if (*p == '.') { p++; while (*p >= '0' && *p <= '9') p++; }
            if (*p == 'e' || *p == 'E') {
                p++;
                if (*p == '+' || *p == '-') p++;
                while (*p >= '0' && *p <= '9') p++;
            }
            r->len = (int)(p - r->tok);
            r->cur = p;
            r->type = '#';
            return '#';
        }
        r->type = -1;
        return -1;
    }
}

/* ================================================================
 *  Token value extraction (from current token, no advance)
 * ================================================================ */

static inline int       jr_as_int(const jr *r)    { return (int)strtol(r->tok, NULL, 10); }
static inline long long jr_as_long(const jr *r)   { return strtoll(r->tok, NULL, 10); }
static inline double    jr_as_double(const jr *r)  { return strtod(r->tok, NULL); }
static inline int       jr_as_bool(const jr *r)   { return r->type == 't'; }

/* copy current string token into buf, handling basic escapes.
 * returns number of bytes written (excluding NUL). */
static inline int jr_as_str(const jr *r, char *buf, int cap) {
    if (r->type != '"' || cap <= 0) { if (cap > 0) buf[0] = 0; return 0; }
    const char *s = r->tok;
    int w = 0, lim = cap - 1;
    for (int i = 0; i < r->len && w < lim; i++) {
        if (s[i] == '\\' && i + 1 < r->len) {
            i++;
            switch (s[i]) {
            case '"':  buf[w++] = '"';  break;
            case '\\': buf[w++] = '\\'; break;
            case '/':  buf[w++] = '/';  break;
            case 'n':  buf[w++] = '\n'; break;
            case 'r':  buf[w++] = '\r'; break;
            case 't':  buf[w++] = '\t'; break;
            default:   buf[w++] = s[i]; break;
            }
        } else {
            buf[w++] = s[i];
        }
    }
    buf[w] = 0;
    return w;
}

/* ================================================================
 *  Key comparison (checks kp/kn set by jr_key)
 * ================================================================ */

static inline int jr_eq(const jr *r, const char *s) {
    return r->kn == (int)strlen(s) && memcmp(r->kp, s, r->kn) == 0;
}

/* ================================================================
 *  Skip a complete value, including nested objects/arrays.
 *  Call when r is positioned ON the value's first token.
 *  After return, r is still on the value's LAST token.
 * ================================================================ */

static inline void jr_skip(jr *r) {
    if (r->type == '{' || r->type == '[') {
        int d = 1;
        while (d > 0 && jr_step(r)) {
            if (r->type == '{' || r->type == '[') d++;
            else if (r->type == '}' || r->type == ']') d--;
        }
    }
}

/* ================================================================
 *  High-level iteration
 *
 *  jr_open(&r)     eat { or [, advance to first inner token (or }/])
 *  jr_close(&r)    step past } or ] into the outer context
 *  jr_key(&r)      object iterator: read next key, eat colon,
 *                  position on value. key available via jr_eq().
 *                  returns 1 if key found, 0 if } reached.
 *  jr_val_*(&r)    read value + advance past it.
 * ================================================================ */

static inline void jr_open(jr *r) {
    if (r->type != '{' && r->type != '[') jr_step(r);
    jr_step(r);
}

static inline void jr_close(jr *r) {
    if (r->type == '}' || r->type == ']') jr_step(r);
}

static inline int jr_key(jr *r) {
    if (r->type == ',') jr_step(r);
    if (r->type == '}' || r->type == 0) return 0;
    if (r->type != '"') return 0;
    r->kp = r->tok;
    r->kn = r->len;
    jr_step(r);   /* : */
    jr_step(r);   /* value */
    return 1;
}

/* read value + step to next token (comma, closing bracket, or end) */
static inline int jr_val_int(jr *r) {
    int v = jr_as_int(r); jr_step(r); return v;
}
static inline long long jr_val_long(jr *r) {
    long long v = jr_as_long(r); jr_step(r); return v;
}
static inline double jr_val_double(jr *r) {
    double v = jr_as_double(r); jr_step(r); return v;
}
static inline int jr_val_bool(jr *r) {
    int v = jr_as_bool(r); jr_step(r); return v;
}
static inline int jr_val_str(jr *r, char *buf, int cap) {
    int n = jr_as_str(r, buf, cap); jr_step(r); return n;
}
static inline void jr_val_skip(jr *r) {
    jr_skip(r); jr_step(r);
}

/* ================================================================
 *  Bulk array readers
 *
 *  Read an entire [...] of homogeneous values into a C array.
 *  Call when r is positioned ON the '[' token.
 *  After return, r is past ']'.
 *  Returns number of elements read.
 *
 *  jr_open(&r);
 *  while (jr_key(&r)) {
 *      if (jr_eq(&r, "data"))  n = jr_val_ints(&r, buf, 1024);
 *      ...
 *  }
 * ================================================================ */

static inline int jr_val_ints(jr *r, int *out, int cap) {
    jr_open(r);
    int n = 0;
    while (r->type == '#' && n < cap) {
        out[n++] = jr_as_int(r);
        jr_step(r);
        if (r->type == ',') jr_step(r);
    }
    jr_close(r);
    return n;
}

static inline int jr_val_longs(jr *r, long long *out, int cap) {
    jr_open(r);
    int n = 0;
    while (r->type == '#' && n < cap) {
        out[n++] = jr_as_long(r);
        jr_step(r);
        if (r->type == ',') jr_step(r);
    }
    jr_close(r);
    return n;
}

static inline int jr_val_doubles(jr *r, double *out, int cap) {
    jr_open(r);
    int n = 0;
    while (r->type == '#' && n < cap) {
        out[n++] = jr_as_double(r);
        jr_step(r);
        if (r->type == ',') jr_step(r);
    }
    jr_close(r);
    return n;
}

#endif /* JR_H */

