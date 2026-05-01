// SPDX-License-Identifier: 0BSD
// Copyright (c) 2026 Arcie Ren

/* jw.h — JSON Writer. C99. Single header. Zero alloc. Zero deps.
 *
 * Three ways to write JSON, all sharing the same stream:
 *
 *   1. X-MACRO structs ("data segment" — compile-time known layout)
 *
 *       #define POINT(X) X(INT,x) X(INT,y) X(STR,label)
 *       JW_STRUCT(Point, POINT)
 *       // JW_FN(Point, POINT) <--- only generate functions for existing struct
 *
 *       Point pt = {1, 2, "A"};
 *       JW_PRINT(Point, &pt);              // stdout
 *       JW_FPRINT(f, Point, &pt);          // any FILE*
 *       Point_jw(&j, &pt);                 // embed in stream
 *
 *   2. Streaming API ("bss segment" — runtime decisions)
 *
 *       jw j = jw_to(stdout);
 *       jw_begin(&j);
 *         jw_kv_int(&j, "x", 1);
 *         jw_kv_str(&j, "s", "hi");
 *         jw_key(&j, "pt"); Point_jw(&j, &pt);   // mix!
 *         if (has_gpu) jw_kv_str(&j, "gpu", name); // conditional!
 *       jw_end(&j);
 *       jw_endl(&j);
 *
 *   3. Runtime property table (dynamic field lists)
 *
 *       jw_prop p[] = {
 *           JW_INT("x", 1), JW_STR("name", user),
 *           JW_DBLS("data", buf, 256),
 *       };
 *       if (summary) p[2].k = NULL;        // skip at runtime
 *       jw_props(&j, p, 3);
 *
 *   Unifying principle: everything calls the same stream primitives.
 *   X-MACRO generates streaming calls at compile time.
 *   Property tables loop streaming calls at runtime.
 *   Hand-written code calls them directly.
 */

#ifndef JW_H
#define JW_H

#include <stdio.h>
#include <string.h>

/* ================================================================
 *  Context
 * ================================================================ */

typedef struct {
    FILE *f;
    char  s[32];   /* nesting stack: '{' or '['                    */
    int   n[32];   /* item count at each depth (drives commas)     */
    int   d;       /* current depth                                */
    int   ak;      /* after-key flag: suppress comma on next value */
} jw;

static inline jw jw_to(FILE *f) {
    jw j;
    memset(&j, 0, sizeof j);
    j.f = f;
    return j;
}

/* ================================================================
 *  Streaming primitives
 * ================================================================ */

/* -- internal helpers -- */

static inline void jw__sep(jw *j) {
    if (j->ak) { j->ak = 0; return; }
    if (j->d > 0 && j->n[j->d]++ > 0) fputc(',', j->f);
}

static inline void jw__esc(FILE *f, const char *s) {
    fputc('"', f);
    for (; *s; s++) {
        switch (*s) {
        case '"':  fputs("\\\"", f); break;
        case '\\': fputs("\\\\", f); break;
        case '\n': fputs("\\n",  f); break;
        case '\r': fputs("\\r",  f); break;
        case '\t': fputs("\\t",  f); break;
        default:
            if ((unsigned char)*s < 0x20)
                fprintf(f, "\\u%04x", (unsigned char)*s);
            else
                fputc(*s, f);
        }
    }
    fputc('"', f);
}

/* -- containers -- */

static inline void jw_begin(jw *j) {
    jw__sep(j);
    fputc('{', j->f);
    j->d++;
    j->s[j->d] = '{';
    j->n[j->d] = 0;
}

static inline void jw_arr(jw *j) {
    jw__sep(j);
    fputc('[', j->f);
    j->d++;
    j->s[j->d] = '[';
    j->n[j->d] = 0;
}

static inline void jw_end(jw *j) {
    fputc(j->s[j->d] == '{' ? '}' : ']', j->f);
    j->d--;
}

/* -- key (object only) -- */

static inline void jw_key(jw *j, const char *k) {
    jw__sep(j);
    jw__esc(j->f, k);
    fputc(':', j->f);
    j->ak = 1;
}

/* -- scalar values -- */

static inline void jw_int(jw *j, long long v)       { jw__sep(j); fprintf(j->f, "%lld", v); }
static inline void jw_double(jw *j, double v)        { jw__sep(j); fprintf(j->f, "%g", v); }
static inline void jw_bool(jw *j, int v)             { jw__sep(j); fputs(v ? "true" : "false", j->f); }
static inline void jw_null(jw *j)                    { jw__sep(j); fputs("null", j->f); }
static inline void jw_raw(jw *j, const char *s)      { jw__sep(j); fputs(s, j->f); }

static inline void jw_str(jw *j, const char *s) {
    jw__sep(j);
    if (s) jw__esc(j->f, s); else fputs("null", j->f);
}

/* -- key + value shortcuts -- */

static inline void jw_kv_int(jw *j, const char *k, long long v)    { jw_key(j,k); jw_int(j,v); }
static inline void jw_kv_double(jw *j, const char *k, double v)    { jw_key(j,k); jw_double(j,v); }
static inline void jw_kv_bool(jw *j, const char *k, int v)         { jw_key(j,k); jw_bool(j,v); }
static inline void jw_kv_null(jw *j, const char *k)                { jw_key(j,k); jw_null(j); }
static inline void jw_kv_str(jw *j, const char *k, const char *s)  { jw_key(j,k); jw_str(j,s); }
static inline void jw_kv_raw(jw *j, const char *k, const char *s)  { jw_key(j,k); jw_raw(j,s); }
static inline void jw_kv_obj(jw *j, const char *k) { jw_key(j,k); jw_begin(j); }
static inline void jw_kv_arr(jw *j, const char *k) { jw_key(j,k); jw_arr(j); }

/* -- bulk arrays -- */

static inline void jw_ints(jw *j, const int *v, int n) {
    jw_arr(j); for (int i = 0; i < n; i++) jw_int(j, v[i]); jw_end(j);
}
static inline void jw_longs(jw *j, const long long *v, int n) {
    jw_arr(j); for (int i = 0; i < n; i++) jw_int(j, v[i]); jw_end(j);
}
static inline void jw_doubles(jw *j, const double *v, int n) {
    jw_arr(j); for (int i = 0; i < n; i++) jw_double(j, v[i]); jw_end(j);
}
static inline void jw_strs(jw *j, const char **v, int n) {
    jw_arr(j); for (int i = 0; i < n; i++) jw_str(j, v[i]); jw_end(j);
}

/* -- finish line: newline + flush (pipe-friendly) -- */

static inline void jw_endl(jw *j) { fputc('\n', j->f); fflush(j->f); }

/* ================================================================
 *  Runtime property table
 *
 *  Build a JSON object from a flat array of key-value pairs.
 *  Skip a field at runtime by setting its .k to NULL.
 *  Reorder by reordering the array.
 *
 *  Types: INT LONG DBL STR BOOL NUL INTS LONGS DBLS STRS RAW
 * ================================================================ */

enum {
    JW_PINT, JW_PLONG, JW_PDBL, JW_PSTR, JW_PBOOL, JW_PNUL,
    JW_PINTS, JW_PLONGS, JW_PDBLS, JW_PSTRS, JW_PRAW,
};

typedef struct {
    const char *k;         /* key (NULL = skip this field) */
    int         t;         /* JW_P* type tag               */
    union {
        long long    i;
        double       d;
        const char  *s;
        struct { const void *p; int n; } a;
    } v;
} jw_prop;

#define JW_INT(key, val)           { (key), JW_PINT,   { .i = (val) } }
#define JW_LONG(key, val)          { (key), JW_PLONG,  { .i = (val) } }
#define JW_DBL(key, val)           { (key), JW_PDBL,   { .d = (val) } }
#define JW_STR(key, val)           { (key), JW_PSTR,   { .s = (val) } }
#define JW_BOOL(key, val)          { (key), JW_PBOOL,  { .i = (val) } }
#define JW_NUL(key)                { (key), JW_PNUL,   { .i = 0 } }
#define JW_RAW(key, val)           { (key), JW_PRAW,   { .s = (val) } }
#define JW_INTS(key, arr, n)       { (key), JW_PINTS,  { .a = { (arr), (n) } } }
#define JW_LONGS(key, arr, n)      { (key), JW_PLONGS, { .a = { (arr), (n) } } }
#define JW_DBLS(key, arr, n)       { (key), JW_PDBLS,  { .a = { (arr), (n) } } }
#define JW_STRS(key, arr, n)       { (key), JW_PSTRS,  { .a = { (arr), (n) } } }

static inline void jw_props(jw *j, const jw_prop *p, int n) {
    jw_begin(j);
    for (int i = 0; i < n; i++) {
        if (!p[i].k) continue;
        jw_key(j, p[i].k);
        switch (p[i].t) {
        case JW_PINT:   jw_int(j, p[i].v.i);                                     break;
        case JW_PLONG:  jw_int(j, p[i].v.i);                                     break;
        case JW_PDBL:   jw_double(j, p[i].v.d);                                  break;
        case JW_PSTR:   jw_str(j, p[i].v.s);                                     break;
        case JW_PBOOL:  jw_bool(j, (int)p[i].v.i);                               break;
        case JW_PNUL:   jw_null(j);                                               break;
        case JW_PRAW:   jw_raw(j, p[i].v.s);                                      break;
        case JW_PINTS:  jw_ints(j, (const int *)p[i].v.a.p, p[i].v.a.n);         break;
        case JW_PLONGS: jw_longs(j, (const long long *)p[i].v.a.p, p[i].v.a.n);  break;
        case JW_PDBLS:  jw_doubles(j, (const double *)p[i].v.a.p, p[i].v.a.n);   break;
        case JW_PSTRS:  jw_strs(j, (const char **)p[i].v.a.p, p[i].v.a.n);       break;
        }
    }
    jw_end(j);
}

#define JW_NPROPS(a) ((int)(sizeof(a)/sizeof(a[0])))

/* ================================================================
 *  X-MACRO struct + serializer generation
 *
 *  Supported tags:
 *    INT     int                IARR   int *m;    int m_n;
 *    LONG    long long          DARR   double *m; int m_n;
 *    DOUBLE  double             SARR   const char **m; int m_n;
 *    STR     const char *
 *    BOOL    int
 * ================================================================ */

/* field declarations */
#define JW__D_INT(m)    int m;
#define JW__D_LONG(m)   long long m;
#define JW__D_DOUBLE(m) double m;
#define JW__D_STR(m)    const char *m;
#define JW__D_BOOL(m)   int m;
#define JW__D_IARR(m)   int *m;            int m##_n;
#define JW__D_DARR(m)   double *m;         int m##_n;
#define JW__D_SARR(m)   const char **m;    int m##_n;
#define JW__DECL(tag, m) JW__D_##tag(m)

/* emit calls */
#define JW__E_INT(m)    jw_kv_int(_j, #m, _o->m);
#define JW__E_LONG(m)   jw_kv_int(_j, #m, _o->m);
#define JW__E_DOUBLE(m) jw_kv_double(_j, #m, _o->m);
#define JW__E_STR(m)    jw_kv_str(_j, #m, _o->m);
#define JW__E_BOOL(m)   jw_kv_bool(_j, #m, _o->m);
#define JW__E_IARR(m)   jw_key(_j, #m); jw_ints(_j, _o->m, _o->m##_n);
#define JW__E_DARR(m)   jw_key(_j, #m); jw_doubles(_j, _o->m, _o->m##_n);
#define JW__E_SARR(m)   jw_key(_j, #m); jw_strs(_j, _o->m, _o->m##_n);
#define JW__EMIT(tag, m) JW__E_##tag(m)

/* generate typedef + Name_jw() */
#define JW_STRUCT(Name, FIELDS)                                            \
    struct Name { FIELDS(JW__DECL) };                                       \
    static inline void Name##_jw(jw *_j, const struct Name *_o) {          \
        jw_begin(_j); FIELDS(JW__EMIT) jw_end(_j);                           \
    }

/* struct already defined elsewhere — only generate serializer */
#define JW_FN(Name, FIELDS)                                                \
    static inline void Name##_jw(jw *_j, const struct Name *_o) {          \
        jw_begin(_j); FIELDS(JW__EMIT) jw_end(_j);                           \
    }

/* one-liner output */
#define JW_PRINT(Type, ptr) do {                                            \
        jw _j = jw_to(stdout);                                              \
        Type##_jw(&_j, (ptr));                                               \
        jw_endl(&_j);                                                          \
    } while (0)

#define JW_FPRINT(file, Type, ptr) do {                                     \
        jw _j = jw_to(file);                                                \
        Type##_jw(&_j, (ptr));                                               \
        jw_endl(&_j);                                                          \
    } while (0)

#endif /* JW_H */

