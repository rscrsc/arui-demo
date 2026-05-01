// SPDX-License-Identifier: 0BSD
// Copyright (c) 2026 Arcie Ren

/* cplot.h — header-only C plotting library
 *
 * Usage:
 *   #define CPLOT_IMPLEMENTATION   (in exactly ONE .c file before including)
 *   #include "cplot.h"
 *
 * Features:
 *   - Line, scatter, bar, histogram, hline/vline, fill_between
 *   - Subplots (rows × cols grid)
 *   - Auto-ranging with nice tick marks
 *   - Built-in 5×7 bitmap font (all printable ASCII)
 *   - Grid, legend, titles, axis labels
 *   - Matplotlib-style format strings ("r-", "b.", "g--o")
 *   - Writes BMP (24-bit) or TGA (24-bit)
 *   - Zero external dependencies (only libc + math)
 *
 * Example:
 *   #define CPLOT_IMPLEMENTATION
 *   #include "cplot.h"
 *   int main(void) {
 *       double x[] = {0,1,2,3,4,5};
 *       double y[] = {0,1,4,9,16,25};
 *       cp_fig *f = cp_figure(640, 480);
 *       cp_axes *ax = cp_subplot(f, 1, 1, 0);
 *       cp_plot(ax, x, y, 6, "b-o|label=x^2|lw=2");
 *       cp_title(ax, "Parabola");
 *       cp_xlabel(ax, "x"); cp_ylabel(ax, "y");
 *       cp_grid(ax, 1); cp_legend(ax);
 *       cp_save(f, "plot.bmp");
 *       cp_free(f);
 *   }
 *
 */
#ifndef CPLOT_H
#define CPLOT_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <float.h>

/* ══════════════════════════════════════════════════════════════════════════
 *  PUBLIC TYPES
 * ══════════════════════════════════════════════════════════════════════════ */

typedef struct { uint8_t r, g, b, a; } cp_color;

#define CP_RGB(R,G,B)     ((cp_color){(R),(G),(B),255})
#define CP_RGBA(R,G,B,A)  ((cp_color){(R),(G),(B),(A)})
#define CP_WHITE   CP_RGB(255,255,255)
#define CP_BLACK   CP_RGB(0,0,0)
#define CP_GRAY    CP_RGB(200,200,200)
#define CP_LTGRAY  CP_RGB(230,230,230)

/* line style */
enum { CP_SOLID=0, CP_DASHED, CP_DOTTED, CP_DASHDOT };
/* marker */
enum { CP_MARKER_NONE=0, CP_MARKER_DOT, CP_MARKER_CIRCLE, CP_MARKER_SQUARE,
       CP_MARKER_PLUS, CP_MARKER_CROSS, CP_MARKER_TRI };
/* series type */
enum { CP_TYPE_LINE=0, CP_TYPE_BAR, CP_TYPE_FILL, CP_TYPE_HLINE, CP_TYPE_VLINE };

typedef struct cp_series {
    double         *x, *y;
    int             n;
    cp_color        color;
    int             line_width;
    int             line_style;
    int             marker;
    int             marker_size;
    char            label[80];
    int             type;
    double          bar_width;
    double          fill_alpha;
    struct cp_series *next;
} cp_series;

typedef struct cp_axes {
    int    px, py, pw, ph;          /* plot area in figure coords */
    double xmin, xmax, ymin, ymax;  /* data range */
    int    auto_x, auto_y;
    char   title_str[128];
    char   xlabel_str[80];
    char   ylabel_str[80];
    int    show_grid;
    int    show_legend;
    cp_series *series_head;
    int    n_series;
    int    subplot_row, subplot_col, subplot_idx;
} cp_axes;

typedef struct cp_fig {
    int       width, height;
    uint8_t  *pixels;               /* RGBA, row-major, top-left origin */
    cp_color  bg;
    char      suptitle_str[128];
    cp_axes **axes;
    int       n_axes, cap_axes;
} cp_fig;

/* ══════════════════════════════════════════════════════════════════════════
 *  PUBLIC API
 * ══════════════════════════════════════════════════════════════════════════ */

static cp_fig   *cp_figure(int width, int height);
static cp_axes  *cp_subplot(cp_fig *fig, int nrows, int ncols, int index);
static void      cp_suptitle(cp_fig *fig, const char *text);

/* data plotting — opts: "r-o|label=name|lw=2|ms=5|bw=0.8|alpha=0.3" */
static void      cp_plot(cp_axes *ax, const double *x, const double *y, int n, const char *opts);
static void      cp_scatter(cp_axes *ax, const double *x, const double *y, int n, const char *opts);
static void      cp_bar(cp_axes *ax, const double *x, const double *y, int n, const char *opts);
static void      cp_hist(cp_axes *ax, const double *data, int n, int bins, const char *opts);
static void      cp_hline(cp_axes *ax, double y, const char *opts);
static void      cp_vline(cp_axes *ax, double x, const char *opts);
static void      cp_fill_between(cp_axes *ax, const double *x,
                                  const double *y1, const double *y2, int n, const char *opts);

/* axis config */
static void      cp_title(cp_axes *ax, const char *text);
static void      cp_xlabel(cp_axes *ax, const char *text);
static void      cp_ylabel(cp_axes *ax, const char *text);
static void      cp_xlim(cp_axes *ax, double lo, double hi);
static void      cp_ylim(cp_axes *ax, double lo, double hi);
static void      cp_grid(cp_axes *ax, int on);
static void      cp_legend(cp_axes *ax);

/* output */
static int       cp_save(cp_fig *fig, const char *filename);
static void      cp_free(cp_fig *fig);

/* ══════════════════════════════════════════════════════════════════════════
 *  IMPLEMENTATION
 * ══════════════════════════════════════════════════════════════════════════ */
#ifdef CPLOT_IMPLEMENTATION

/* ── palette (matplotlib tab10) ────────────────────────────────────────── */
static const cp_color cp__palette[] = {
    {31,119,180,255}, {255,127,14,255}, {44,160,44,255}, {214,39,40,255},
    {148,103,189,255},{140,86,75,255},  {227,119,194,255},{127,127,127,255},
    {188,189,34,255}, {23,190,207,255},
};
#define CP__NPALETTE (int)(sizeof(cp__palette)/sizeof(cp__palette[0]))

/* ── built-in 5×7 bitmap font (ASCII 32-126, column-major, LSB=top) ──── */
static const uint8_t cp__font5x7[95][5] = {
    {0x00,0x00,0x00,0x00,0x00},/*   */ {0x00,0x00,0x5F,0x00,0x00},/* ! */
    {0x00,0x07,0x00,0x07,0x00},/* " */ {0x14,0x7F,0x14,0x7F,0x14},/* # */
    {0x24,0x2A,0x7F,0x2A,0x12},/* $ */ {0x23,0x13,0x08,0x64,0x62},/* % */
    {0x36,0x49,0x55,0x22,0x50},/* & */ {0x00,0x00,0x07,0x00,0x00},/* ' */
    {0x00,0x1C,0x22,0x41,0x00},/* ( */ {0x00,0x41,0x22,0x1C,0x00},/* ) */
    {0x14,0x08,0x3E,0x08,0x14},/* * */ {0x08,0x08,0x3E,0x08,0x08},/* + */
    {0x00,0x50,0x30,0x00,0x00},/* , */ {0x08,0x08,0x08,0x08,0x08},/* - */
    {0x00,0x60,0x60,0x00,0x00},/* . */ {0x20,0x10,0x08,0x04,0x02},/* / */
    {0x3E,0x51,0x49,0x45,0x3E},/* 0 */ {0x00,0x42,0x7F,0x40,0x00},/* 1 */
    {0x42,0x61,0x51,0x49,0x46},/* 2 */ {0x21,0x41,0x45,0x4B,0x31},/* 3 */
    {0x18,0x14,0x12,0x7F,0x10},/* 4 */ {0x27,0x45,0x45,0x45,0x39},/* 5 */
    {0x3C,0x4A,0x49,0x49,0x30},/* 6 */ {0x01,0x71,0x09,0x05,0x03},/* 7 */
    {0x36,0x49,0x49,0x49,0x36},/* 8 */ {0x06,0x49,0x49,0x29,0x1E},/* 9 */
    {0x00,0x36,0x36,0x00,0x00},/* : */ {0x00,0x56,0x36,0x00,0x00},/* ; */
    {0x08,0x14,0x22,0x41,0x00},/* < */ {0x14,0x14,0x14,0x14,0x14},/* = */
    {0x00,0x41,0x22,0x14,0x08},/* > */ {0x02,0x01,0x51,0x09,0x06},/* ? */
    {0x3E,0x41,0x5D,0x55,0x1E},/* @ */ {0x7E,0x09,0x09,0x09,0x7E},/* A */
    {0x7F,0x49,0x49,0x49,0x36},/* B */ {0x3E,0x41,0x41,0x41,0x22},/* C */
    {0x7F,0x41,0x41,0x22,0x1C},/* D */ {0x7F,0x49,0x49,0x49,0x41},/* E */
    {0x7F,0x09,0x09,0x09,0x01},/* F */ {0x3E,0x41,0x49,0x49,0x7A},/* G */
    {0x7F,0x08,0x08,0x08,0x7F},/* H */ {0x00,0x41,0x7F,0x41,0x00},/* I */
    {0x20,0x40,0x41,0x3F,0x01},/* J */ {0x7F,0x08,0x14,0x22,0x41},/* K */
    {0x7F,0x40,0x40,0x40,0x40},/* L */ {0x7F,0x02,0x0C,0x02,0x7F},/* M */
    {0x7F,0x04,0x08,0x10,0x7F},/* N */ {0x3E,0x41,0x41,0x41,0x3E},/* O */
    {0x7F,0x09,0x09,0x09,0x06},/* P */ {0x3E,0x41,0x51,0x21,0x5E},/* Q */
    {0x7F,0x09,0x19,0x29,0x46},/* R */ {0x46,0x49,0x49,0x49,0x31},/* S */
    {0x01,0x01,0x7F,0x01,0x01},/* T */ {0x3F,0x40,0x40,0x40,0x3F},/* U */
    {0x1F,0x20,0x40,0x20,0x1F},/* V */ {0x3F,0x40,0x38,0x40,0x3F},/* W */
    {0x63,0x14,0x08,0x14,0x63},/* X */ {0x07,0x08,0x70,0x08,0x07},/* Y */
    {0x61,0x51,0x49,0x45,0x43},/* Z */ {0x00,0x7F,0x41,0x41,0x00},/* [ */
    {0x02,0x04,0x08,0x10,0x20},/* \ */ {0x00,0x41,0x41,0x7F,0x00},/* ] */
    {0x04,0x02,0x01,0x02,0x04},/* ^ */ {0x40,0x40,0x40,0x40,0x40},/* _ */
    {0x00,0x01,0x02,0x04,0x00},/* ` */ {0x20,0x54,0x54,0x54,0x78},/* a */
    {0x7F,0x48,0x44,0x44,0x38},/* b */ {0x38,0x44,0x44,0x44,0x20},/* c */
    {0x38,0x44,0x44,0x48,0x7F},/* d */ {0x38,0x54,0x54,0x54,0x18},/* e */
    {0x08,0x7E,0x09,0x01,0x02},/* f */ {0x0C,0x52,0x52,0x52,0x3E},/* g */
    {0x7F,0x08,0x04,0x04,0x78},/* h */ {0x00,0x44,0x7D,0x40,0x00},/* i */
    {0x20,0x40,0x44,0x3D,0x00},/* j */ {0x7F,0x10,0x28,0x44,0x00},/* k */
    {0x00,0x41,0x7F,0x40,0x00},/* l */ {0x7C,0x04,0x18,0x04,0x78},/* m */
    {0x7C,0x08,0x04,0x04,0x78},/* n */ {0x38,0x44,0x44,0x44,0x38},/* o */
    {0x7C,0x14,0x14,0x14,0x08},/* p */ {0x08,0x14,0x14,0x18,0x7C},/* q */
    {0x7C,0x08,0x04,0x04,0x08},/* r */ {0x48,0x54,0x54,0x54,0x20},/* s */
    {0x04,0x3F,0x44,0x40,0x20},/* t */ {0x3C,0x40,0x40,0x20,0x7C},/* u */
    {0x1C,0x20,0x40,0x20,0x1C},/* v */ {0x3C,0x40,0x30,0x40,0x3C},/* w */
    {0x44,0x28,0x10,0x28,0x44},/* x */ {0x0C,0x50,0x50,0x50,0x3C},/* y */
    {0x44,0x64,0x54,0x4C,0x44},/* z */ {0x00,0x08,0x36,0x41,0x00},/* { */
    {0x00,0x00,0x7F,0x00,0x00},/* | */ {0x00,0x41,0x36,0x08,0x00},/* } */
    {0x08,0x04,0x08,0x10,0x08},/* ~ */
};

#define CP__FONT_W 5
#define CP__FONT_H 7
#define CP__CHAR_W 6  /* 5 + 1 spacing */
#define CP__CHAR_H 9  /* 7 + 2 spacing */

/* ── layout constants ──────────────────────────────────────────────────── */
#define CP__MARGIN_L   70   /* left (room for y-label + ticks) */
#define CP__MARGIN_R   20
#define CP__MARGIN_T   40   /* top (room for title) */
#define CP__MARGIN_B   50   /* bottom (room for x-label + ticks) */
#define CP__SUP_H      25   /* suptitle height */

/* ── pixel operations ──────────────────────────────────────────────────── */

static inline void cp__put(cp_fig *f, int x, int y, cp_color c) {
    if (x < 0 || x >= f->width || y < 0 || y >= f->height) return;
    int i = (y * f->width + x) * 4;
    f->pixels[i]   = c.r;
    f->pixels[i+1] = c.g;
    f->pixels[i+2] = c.b;
    f->pixels[i+3] = c.a;
}

static inline void cp__blend(cp_fig *f, int x, int y, cp_color c, double alpha) {
    if (x < 0 || x >= f->width || y < 0 || y >= f->height) return;
    int i = (y * f->width + x) * 4;
    uint8_t a = (uint8_t)(alpha * c.a);
    f->pixels[i]   = (uint8_t)((c.r * a + f->pixels[i]   * (255 - a)) / 255);
    f->pixels[i+1] = (uint8_t)((c.g * a + f->pixels[i+1] * (255 - a)) / 255);
    f->pixels[i+2] = (uint8_t)((c.b * a + f->pixels[i+2] * (255 - a)) / 255);
    f->pixels[i+3] = 255;
}

/* ── drawing primitives ────────────────────────────────────────────────── */

static void cp__line(cp_fig *f, int x0, int y0, int x1, int y1,
                     cp_color c, int thick, int style)
{
    int dx = abs(x1-x0), sx = x0<x1 ? 1 : -1;
    int dy = -abs(y1-y0), sy = y0<y1 ? 1 : -1;
    int err = dx+dy, e2;
    int step = 0, dash_len = (style==CP_DASHED) ? 8 : (style==CP_DOTTED) ? 2 : 6;
    int gap_len  = (style==CP_DASHED) ? 5 : (style==CP_DOTTED) ? 3 : 4;

    for (;;) {
        int draw = 1;
        if (style != CP_SOLID) {
            int phase = step % (dash_len + gap_len);
            if (style == CP_DASHDOT) {
                int cycle = step % (dash_len + gap_len + 2 + gap_len);
                draw = (cycle < dash_len) || (cycle >= dash_len+gap_len && cycle < dash_len+gap_len+2);
            } else {
                draw = (phase < dash_len);
            }
        }
        if (draw) {
            for (int t = -(thick/2); t <= thick/2; t++) {
                if (abs(dy) > abs(dx))
                    cp__put(f, x0+t, y0, c);
                else
                    cp__put(f, x0, y0+t, c);
            }
        }
        if (x0==x1 && y0==y1) break;
        e2 = 2*err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
        step++;
    }
}

static void cp__fill_rect(cp_fig *f, int x0, int y0, int x1, int y1, cp_color c) {
    if (x0>x1) { int t=x0; x0=x1; x1=t; }
    if (y0>y1) { int t=y0; y0=y1; y1=t; }
    for (int y=y0; y<=y1; y++)
        for (int x=x0; x<=x1; x++)
            cp__put(f, x, y, c);
}

static void cp__fill_rect_alpha(cp_fig *f, int x0, int y0, int x1, int y1,
                                 cp_color c, double alpha) {
    if (x0>x1) { int t=x0; x0=x1; x1=t; }
    if (y0>y1) { int t=y0; y0=y1; y1=t; }
    for (int y=y0; y<=y1; y++)
        for (int x=x0; x<=x1; x++)
            cp__blend(f, x, y, c, alpha);
}

static void cp__rect(cp_fig *f, int x0, int y0, int x1, int y1, cp_color c, int thick) {
    cp__line(f, x0,y0, x1,y0, c, thick, CP_SOLID);
    cp__line(f, x1,y0, x1,y1, c, thick, CP_SOLID);
    cp__line(f, x1,y1, x0,y1, c, thick, CP_SOLID);
    cp__line(f, x0,y1, x0,y0, c, thick, CP_SOLID);
}

static void cp__fill_circle(cp_fig *f, int cx, int cy, int r, cp_color c) {
    for (int dy=-r; dy<=r; dy++)
        for (int dx=-r; dx<=r; dx++)
            if (dx*dx+dy*dy <= r*r)
                cp__put(f, cx+dx, cy+dy, c);
}

static void cp__circle(cp_fig *f, int cx, int cy, int r, cp_color c) {
    for (int dy=-r; dy<=r; dy++)
        for (int dx=-r; dx<=r; dx++) {
            int d2 = dx*dx+dy*dy;
            if (d2 <= r*r && d2 >= (r-1)*(r-1))
                cp__put(f, cx+dx, cy+dy, c);
        }
}

/* ── text rendering ────────────────────────────────────────────────────── */

static void cp__char(cp_fig *f, int x, int y, char ch, cp_color c, int scale) {
    if (ch < 32 || ch > 126) ch = '?';
    const uint8_t *g = cp__font5x7[ch - 32];
    for (int col = 0; col < CP__FONT_W; col++)
        for (int row = 0; row < CP__FONT_H; row++)
            if (g[col] & (1 << row))
                for (int sy=0; sy<scale; sy++)
                    for (int sx=0; sx<scale; sx++)
                        cp__put(f, x+col*scale+sx, y+row*scale+sy, c);
}

static void cp__text(cp_fig *f, int x, int y, const char *s, cp_color c, int scale) {
    for (; *s; s++, x += CP__CHAR_W * scale)
        cp__char(f, x, y, *s, c, scale);
}

static void cp__text_centered(cp_fig *f, int cx, int y, const char *s, cp_color c, int scale) {
    int w = (int)strlen(s) * CP__CHAR_W * scale;
    cp__text(f, cx - w/2, y, s, c, scale);
}

static void cp__text_right(cp_fig *f, int rx, int y, const char *s, cp_color c, int scale) {
    int w = (int)strlen(s) * CP__CHAR_W * scale;
    cp__text(f, rx - w, y, s, c, scale);
}

/* vertical text (90° CCW rotation) */
static void cp__char_v(cp_fig *f, int x, int y, char ch, cp_color c, int scale) {
    if (ch < 32 || ch > 126) ch = '?';
    const uint8_t *g = cp__font5x7[ch - 32];
    for (int col = 0; col < CP__FONT_W; col++)
        for (int row = 0; row < CP__FONT_H; row++)
            if (g[col] & (1 << row))
                for (int sy=0; sy<scale; sy++)
                    for (int sx=0; sx<scale; sx++)
                        cp__put(f, x+row*scale+sy, y-col*scale-sx, c);
}

static void cp__text_v(cp_fig *f, int x, int cy, const char *s, cp_color c, int scale) {
    int len = (int)strlen(s);
    int h = len * CP__CHAR_W * scale;
    int y = cy + h/2;
    for (; *s; s++, y -= CP__CHAR_W * scale)
        cp__char_v(f, x, y, *s, c, scale);
}

/* ── color / option parsing ────────────────────────────────────────────── */

static cp_color cp__color_from_char(char c) {
    switch(c) {
    case 'r': return CP_RGB(214,39,40);
    case 'g': return CP_RGB(44,160,44);
    case 'b': return CP_RGB(31,119,180);
    case 'c': return CP_RGB(23,190,207);
    case 'm': return CP_RGB(148,103,189);
    case 'y': return CP_RGB(188,189,34);
    case 'k': return CP_RGB(0,0,0);
    case 'w': return CP_RGB(255,255,255);
    default:  return CP_RGB(0,0,0);
    }
}

static cp_color cp__hex_color(const char *h) {
    unsigned int v = 0;
    if (*h == '#') h++;
    for (int i=0; i<6 && h[i]; i++) {
        v <<= 4;
        char c = h[i];
        if (c>='0'&&c<='9') v|=(c-'0');
        else if (c>='a'&&c<='f') v|=(c-'a'+10);
        else if (c>='A'&&c<='F') v|=(c-'A'+10);
    }
    return CP_RGB((v>>16)&0xFF, (v>>8)&0xFF, v&0xFF);
}

typedef struct {
    cp_color color;
    int      has_color;
    int      line_style;
    int      has_line;
    int      marker;
    int      line_width;
    int      marker_size;
    double   bar_width;
    double   alpha;
    char     label[80];
} cp__opts;

static cp__opts cp__parse_opts(const char *s) {
    cp__opts o = {0};
    o.color = CP_BLACK;
    o.has_color = 0;
    o.line_style = -1; /* unset */
    o.has_line = 0;
    o.marker = CP_MARKER_NONE;
    o.line_width = 1;
    o.marker_size = 4;
    o.bar_width = 0.8;
    o.alpha = 1.0;
    o.label[0] = '\0';
    if (!s) return o;

    /* parse short fmt part (before first |) */
    const char *p = s;
    while (*p && *p != '|') {
        switch(*p) {
        case 'r': case 'g': case 'b': case 'c': case 'm': case 'y': case 'k': case 'w':
            o.color = cp__color_from_char(*p); o.has_color = 1; break;
        case '-':
            if (*(p+1)=='-') { o.line_style = CP_DASHED; o.has_line=1; p++; }
            else if (*(p+1)=='.') { o.line_style = CP_DASHDOT; o.has_line=1; p++; }
            else { o.line_style = CP_SOLID; o.has_line=1; }
            break;
        case ':': o.line_style = CP_DOTTED; o.has_line=1; break;
        case '.': o.marker = CP_MARKER_DOT; break;
        case 'o': o.marker = CP_MARKER_CIRCLE; break;
        case 's': o.marker = CP_MARKER_SQUARE; break;
        case '+': o.marker = CP_MARKER_PLUS; break;
        case 'x': o.marker = CP_MARKER_CROSS; break;
        case '^': o.marker = CP_MARKER_TRI; break;
        default: break;
        }
        p++;
    }

    /* parse named options: |key=val|key=val */
    while (*p == '|') {
        p++;
        if (strncmp(p,"lw=",3)==0)    { o.line_width = atoi(p+3); }
        else if (strncmp(p,"ms=",3)==0){ o.marker_size = atoi(p+3); }
        else if (strncmp(p,"bw=",3)==0){ o.bar_width = atof(p+3); }
        else if (strncmp(p,"alpha=",6)==0){ o.alpha = atof(p+6); }
        else if (strncmp(p,"label=",6)==0) {
            const char *v = p+6;
            int i=0;
            while (*v && *v != '|' && i < 79) o.label[i++] = *v++;
            o.label[i] = '\0';
        }
        else if (strncmp(p,"color=#",7)==0) {
            o.color = cp__hex_color(p+6); o.has_color = 1;
        }
        while (*p && *p != '|') p++;
    }
    return o;
}

/* ── nice ticks ────────────────────────────────────────────────────────── */

static double cp__nice_ceil(double x, int round_flag) {
    double e = floor(log10(fabs(x)));
    double f = x / pow(10, e);
    double nf;
    if (round_flag) {
        if (f < 1.5) nf = 1; else if (f < 3) nf = 2;
        else if (f < 7) nf = 5; else nf = 10;
    } else {
        if (f <= 1) nf = 1; else if (f <= 2) nf = 2;
        else if (f <= 5) nf = 5; else nf = 10;
    }
    return nf * pow(10, e);
}

static void cp__nice_ticks(double lo, double hi, int max_ticks,
                            double *ticks, int *n_ticks)
{
    if (hi <= lo) { hi = lo + 1; }
    double range = cp__nice_ceil(hi - lo, 0);
    double d = cp__nice_ceil(range / (max_ticks - 1), 1);
    double gmin = floor(lo / d) * d;
    double gmax = ceil(hi / d) * d;
    int n = 0;
    for (double v = gmin; v <= gmax + 0.5*d && n < 32; v += d)
        if (v >= lo - d*0.01 && v <= hi + d*0.01)
            ticks[n++] = v;
    *n_ticks = n;
}

static void cp__tick_label(double v, char *buf, int len) {
    double av = fabs(v);
    if (av == 0)                  snprintf(buf, len, "0");
    else if (av >= 1e6)           snprintf(buf, len, "%.2g", v);
    else if (av >= 100)           snprintf(buf, len, "%.0f", v);
    else if (av >= 1)             snprintf(buf, len, "%.1f", v);
    else if (av >= 0.01)          snprintf(buf, len, "%.2f", v);
    else                          snprintf(buf, len, "%.2g", v);
    /* strip trailing zeros after decimal */
    char *dot = strchr(buf, '.');
    if (dot) {
        char *e = buf + strlen(buf) - 1;
        while (e > dot && *e == '0') *e-- = '\0';
        if (e == dot) *e = '\0';
    }
}

/* ── coordinate mapping ────────────────────────────────────────────────── */

static inline int cp__mx(cp_axes *ax, double v) {
    return ax->px + (int)((v - ax->xmin) / (ax->xmax - ax->xmin) * ax->pw);
}
static inline int cp__my(cp_axes *ax, double v) {
    return ax->py + ax->ph - (int)((v - ax->ymin) / (ax->ymax - ax->ymin) * ax->ph);
}

/* ── series creation ───────────────────────────────────────────────────── */

static cp_series *cp__new_series(cp_axes *ax, const double *x, const double *y,
                                  int n, cp__opts *o, int type)
{
    cp_series *s = (cp_series*)calloc(1, sizeof(cp_series));
    if (!s) return NULL;
    if (n > 0 && x) {
        s->x = (double*)malloc(n * sizeof(double));
        memcpy(s->x, x, n * sizeof(double));
    }
    if (n > 0 && y) {
        s->y = (double*)malloc(n * sizeof(double));
        memcpy(s->y, y, n * sizeof(double));
    }
    s->n = n;
    s->type = type;
    s->color = o->has_color ? o->color : cp__palette[ax->n_series % CP__NPALETTE];
    s->line_style = o->has_line ? o->line_style : (type==CP_TYPE_LINE ? CP_SOLID : -1);
    s->line_width = o->line_width;
    s->marker = o->marker;
    s->marker_size = o->marker_size;
    s->bar_width = o->bar_width;
    s->fill_alpha = o->alpha;
    if (o->label[0]) strncpy(s->label, o->label, sizeof(s->label)-1);

    /* update data range */
    for (int i = 0; i < n; i++) {
        if (x) {
            double xv = x[i];
            if (ax->n_series==0 && i==0 && ax->auto_x) { ax->xmin=ax->xmax=xv; }
            if (xv < ax->xmin) ax->xmin = xv;
            if (xv > ax->xmax) ax->xmax = xv;
        }
        if (y) {
            double yv = y[i];
            if (ax->n_series==0 && i==0 && ax->auto_y) { ax->ymin=ax->ymax=yv; }
            if (yv < ax->ymin) ax->ymin = yv;
            if (yv > ax->ymax) ax->ymax = yv;
        }
    }

    /* append to list */
    s->next = NULL;
    if (!ax->series_head) ax->series_head = s;
    else {
        cp_series *tail = ax->series_head;
        while (tail->next) tail = tail->next;
        tail->next = s;
    }
    ax->n_series++;
    return s;
}

/* ── marker drawing ────────────────────────────────────────────────────── */

static void cp__draw_marker(cp_fig *f, int x, int y, int marker, int sz, cp_color c) {
    switch (marker) {
    case CP_MARKER_DOT:
        cp__fill_circle(f, x, y, sz/2 > 0 ? sz/2 : 1, c); break;
    case CP_MARKER_CIRCLE:
        cp__circle(f, x, y, sz/2 > 0 ? sz/2 : 2, c); break;
    case CP_MARKER_SQUARE:
        cp__rect(f, x-sz/2, y-sz/2, x+sz/2, y+sz/2, c, 1); break;
    case CP_MARKER_PLUS:
        cp__line(f, x-sz/2,y, x+sz/2,y, c,1,CP_SOLID);
        cp__line(f, x,y-sz/2, x,y+sz/2, c,1,CP_SOLID); break;
    case CP_MARKER_CROSS:
        cp__line(f, x-sz/2,y-sz/2, x+sz/2,y+sz/2, c,1,CP_SOLID);
        cp__line(f, x+sz/2,y-sz/2, x-sz/2,y+sz/2, c,1,CP_SOLID); break;
    case CP_MARKER_TRI:
        cp__line(f, x,y-sz/2, x-sz/2,y+sz/2, c,1,CP_SOLID);
        cp__line(f, x-sz/2,y+sz/2, x+sz/2,y+sz/2, c,1,CP_SOLID);
        cp__line(f, x+sz/2,y+sz/2, x,y-sz/2, c,1,CP_SOLID); break;
    default: break;
    }
}

/* ── rendering ─────────────────────────────────────────────────────────── */

static void cp__render_series(cp_fig *f, cp_axes *ax, cp_series *s) {
    if (s->type == CP_TYPE_HLINE) {
        int yy = cp__my(ax, s->y[0]);
        cp__line(f, ax->px, yy, ax->px+ax->pw, yy, s->color, s->line_width,
                 s->line_style >= 0 ? s->line_style : CP_DASHED);
        return;
    }
    if (s->type == CP_TYPE_VLINE) {
        int xx = cp__mx(ax, s->x[0]);
        cp__line(f, xx, ax->py, xx, ax->py+ax->ph, s->color, s->line_width,
                 s->line_style >= 0 ? s->line_style : CP_DASHED);
        return;
    }
    if (s->type == CP_TYPE_BAR) {
        double bw = s->bar_width;
        for (int i = 0; i < s->n; i++) {
            int xl = cp__mx(ax, s->x[i] - bw/2);
            int xr = cp__mx(ax, s->x[i] + bw/2);
            int yb = cp__my(ax, 0 > ax->ymin ? 0 : ax->ymin);
            int yt = cp__my(ax, s->y[i]);
            if (s->fill_alpha < 1.0)
                cp__fill_rect_alpha(f, xl, yt, xr, yb, s->color, s->fill_alpha);
            else
                cp__fill_rect(f, xl, yt, xr, yb, s->color);
            cp__rect(f, xl, yt, xr, yb, s->color, 1);
        }
        return;
    }
    if (s->type == CP_TYPE_FILL) {
        /* y = lower, x stored separately; uses paired y values in x and y arrays
           Actually: x=x, y=y1, and we stored y2 offset. For simplicity,
           fill_between stores y1 in y, y2 at y[n..2n-1] reusing extra alloc */
        for (int i = 0; i < s->n/2; i++) {
            int xx = cp__mx(ax, s->x[i]);
            int y1 = cp__my(ax, s->y[i]);
            int y2 = cp__my(ax, s->y[i + s->n/2]);
            if (y1 > y2) { int t=y1; y1=y2; y2=t; }
            for (int yy=y1; yy<=y2; yy++)
                cp__blend(f, xx, yy, s->color, s->fill_alpha);
        }
        return;
    }

    /* line + markers (CP_TYPE_LINE) */
    if (s->line_style >= 0 && s->n > 1) {
        for (int i = 0; i < s->n - 1; i++) {
            int x0 = cp__mx(ax, s->x[i]),   y0 = cp__my(ax, s->y[i]);
            int x1 = cp__mx(ax, s->x[i+1]), y1 = cp__my(ax, s->y[i+1]);
            cp__line(f, x0, y0, x1, y1, s->color, s->line_width, s->line_style);
        }
    }
    if (s->marker != CP_MARKER_NONE) {
        for (int i = 0; i < s->n; i++) {
            int px = cp__mx(ax, s->x[i]);
            int py = cp__my(ax, s->y[i]);
            cp__draw_marker(f, px, py, s->marker, s->marker_size, s->color);
        }
    }
}

static void cp__clip_begin(cp_fig *f, cp_axes *ax) { (void)f; (void)ax; }
static void cp__clip_end(cp_fig *f) { (void)f; }

static void cp__render_axes(cp_fig *f, cp_axes *ax) {
    /* finalize ranges */
    if (ax->auto_x) {
        double pad = (ax->xmax - ax->xmin) * 0.05;
        if (pad == 0) pad = 0.5;
        ax->xmin -= pad; ax->xmax += pad;
    }
    if (ax->auto_y) {
        double pad = (ax->ymax - ax->ymin) * 0.05;
        if (pad == 0) pad = 0.5;
        /* include 0 for bar charts */
        int has_bar = 0;
        for (cp_series *s = ax->series_head; s; s=s->next)
            if (s->type == CP_TYPE_BAR) has_bar = 1;
        if (has_bar && ax->ymin > 0) ax->ymin = 0;
        if (has_bar) { pad = (ax->ymax - ax->ymin) * 0.05; if(pad==0)pad=0.5; }
        ax->ymin -= pad; ax->ymax += pad;
    }

    /* white plot background */
    cp__fill_rect(f, ax->px, ax->py, ax->px+ax->pw, ax->py+ax->ph, CP_WHITE);

    /* grid + ticks */
    double xticks[32], yticks[32];
    int nxt, nyt;
    cp__nice_ticks(ax->xmin, ax->xmax, 8, xticks, &nxt);
    cp__nice_ticks(ax->ymin, ax->ymax, 6, yticks, &nyt);

    for (int i = 0; i < nxt; i++) {
        int px = cp__mx(ax, xticks[i]);
        if (px < ax->px || px > ax->px + ax->pw) continue;
        if (ax->show_grid)
            cp__line(f, px, ax->py, px, ax->py+ax->ph, CP_LTGRAY, 1, CP_SOLID);
        cp__line(f, px, ax->py+ax->ph, px, ax->py+ax->ph+4, CP_BLACK, 1, CP_SOLID);
        char buf[32];
        cp__tick_label(xticks[i], buf, sizeof(buf));
        cp__text_centered(f, px, ax->py+ax->ph+7, buf, CP_BLACK, 1);
    }
    for (int i = 0; i < nyt; i++) {
        int py = cp__my(ax, yticks[i]);
        if (py < ax->py || py > ax->py + ax->ph) continue;
        if (ax->show_grid)
            cp__line(f, ax->px, py, ax->px+ax->pw, py, CP_LTGRAY, 1, CP_SOLID);
        cp__line(f, ax->px-4, py, ax->px, py, CP_BLACK, 1, CP_SOLID);
        char buf[32];
        cp__tick_label(yticks[i], buf, sizeof(buf));
        cp__text_right(f, ax->px-6, py - CP__FONT_H/2, buf, CP_BLACK, 1);
    }

    /* render series */
    for (cp_series *s = ax->series_head; s; s = s->next)
        cp__render_series(f, ax, s);

    /* border */
    cp__rect(f, ax->px, ax->py, ax->px+ax->pw, ax->py+ax->ph, CP_BLACK, 1);

    /* title */
    if (ax->title_str[0])
        cp__text_centered(f, ax->px + ax->pw/2, ax->py - 20,
                          ax->title_str, CP_BLACK, 1);
    /* xlabel */
    if (ax->xlabel_str[0])
        cp__text_centered(f, ax->px + ax->pw/2, ax->py + ax->ph + 28,
                          ax->xlabel_str, CP_BLACK, 1);
    /* ylabel (vertical) */
    if (ax->ylabel_str[0])
        cp__text_v(f, ax->px - 58, ax->py + ax->ph/2,
                   ax->ylabel_str, CP_BLACK, 1);

    /* legend */
    if (ax->show_legend) {
        int nlabeled = 0;
        for (cp_series *s = ax->series_head; s; s=s->next)
            if (s->label[0]) nlabeled++;
        if (nlabeled > 0) {
            int lx = ax->px + ax->pw - 10;
            int ly = ax->py + 8;
            int lw = 0;
            /* measure max label width */
            for (cp_series *s = ax->series_head; s; s=s->next)
                if (s->label[0]) {
                    int w = (int)strlen(s->label) * CP__CHAR_W + 25;
                    if (w > lw) lw = w;
                }
            int lh = nlabeled * (CP__CHAR_H + 2) + 6;
            lx -= lw;
            /* background */
            cp__fill_rect(f, lx-4, ly-3, lx+lw+2, ly+lh, CP_RGBA(255,255,255,230));
            cp__rect(f, lx-4, ly-3, lx+lw+2, ly+lh, CP_GRAY, 1);
            /* entries */
            int yy = ly;
            for (cp_series *s = ax->series_head; s; s=s->next) {
                if (!s->label[0]) continue;
                cp__line(f, lx, yy+CP__FONT_H/2, lx+16, yy+CP__FONT_H/2,
                         s->color, 2, s->line_style>=0?s->line_style:CP_SOLID);
                if (s->marker)
                    cp__draw_marker(f, lx+8, yy+CP__FONT_H/2, s->marker, s->marker_size, s->color);
                cp__text(f, lx+22, yy, s->label, CP_BLACK, 1);
                yy += CP__CHAR_H + 2;
            }
        }
    }
}

static void cp__render(cp_fig *f) {
    /* background */
    cp_color bg = f->bg;
    for (int i = 0; i < f->width * f->height; i++) {
        f->pixels[i*4]   = bg.r;
        f->pixels[i*4+1] = bg.g;
        f->pixels[i*4+2] = bg.b;
        f->pixels[i*4+3] = bg.a;
    }

    /* suptitle */
    if (f->suptitle_str[0])
        cp__text_centered(f, f->width/2, 6, f->suptitle_str, CP_BLACK, 2);

    /* axes */
    for (int i = 0; i < f->n_axes; i++)
        cp__render_axes(f, f->axes[i]);
}

/* ── BMP writer ────────────────────────────────────────────────────────── */

static int cp__write_bmp(const char *path, const uint8_t *px, int w, int h) {
    FILE *fp = fopen(path, "wb");
    if (!fp) return -1;
    int row_bytes = w * 3;
    int pad = (4 - (row_bytes % 4)) % 4;
    int img_size = (row_bytes + pad) * h;
    int file_size = 54 + img_size;

    /* file header */
    uint8_t bfh[14] = {'B','M'};
    bfh[2]=(uint8_t)file_size; bfh[3]=(uint8_t)(file_size>>8);
    bfh[4]=(uint8_t)(file_size>>16); bfh[5]=(uint8_t)(file_size>>24);
    bfh[10]=54;
    fwrite(bfh, 1, 14, fp);

    /* DIB header (BITMAPINFOHEADER) */
    uint8_t dib[40] = {0};
    dib[0]=40;
    dib[4]=(uint8_t)w; dib[5]=(uint8_t)(w>>8); dib[6]=(uint8_t)(w>>16); dib[7]=(uint8_t)(w>>24);
    dib[8]=(uint8_t)h; dib[9]=(uint8_t)(h>>8); dib[10]=(uint8_t)(h>>16); dib[11]=(uint8_t)(h>>24);
    dib[12]=1; dib[14]=24;
    dib[20]=(uint8_t)img_size; dib[21]=(uint8_t)(img_size>>8);
    dib[22]=(uint8_t)(img_size>>16); dib[23]=(uint8_t)(img_size>>24);
    fwrite(dib, 1, 40, fp);

    /* pixel data (bottom-up, BGR) */
    uint8_t zeros[4] = {0};
    for (int y = h-1; y >= 0; y--) {
        for (int x = 0; x < w; x++) {
            int i = (y * w + x) * 4;
            uint8_t bgr[3] = { px[i+2], px[i+1], px[i] };
            fwrite(bgr, 1, 3, fp);
        }
        if (pad) fwrite(zeros, 1, pad, fp);
    }
    fclose(fp);
    return 0;
}

/* ── TGA writer ────────────────────────────────────────────────────────── */

static int cp__write_tga(const char *path, const uint8_t *px, int w, int h) {
    FILE *fp = fopen(path, "wb");
    if (!fp) return -1;
    uint8_t hdr[18] = {0};
    hdr[2] = 2; /* uncompressed true-color */
    hdr[12]=(uint8_t)w; hdr[13]=(uint8_t)(w>>8);
    hdr[14]=(uint8_t)h; hdr[15]=(uint8_t)(h>>8);
    hdr[16]=24;
    fwrite(hdr, 1, 18, fp);
    for (int y = h-1; y >= 0; y--)
        for (int x = 0; x < w; x++) {
            int i = (y * w + x) * 4;
            uint8_t bgr[3] = { px[i+2], px[i+1], px[i] };
            fwrite(bgr, 1, 3, fp);
        }
    fclose(fp);
    return 0;
}

/* ── Public API implementation ─────────────────────────────────────────── */

static cp_fig *cp_figure(int width, int height) {
    cp_fig *f = (cp_fig*)calloc(1, sizeof(cp_fig));
    if (!f) return NULL;
    f->width = width;
    f->height = height;
    f->pixels = (uint8_t*)calloc(width * height * 4, 1);
    f->bg = CP_RGB(245, 245, 245);
    f->cap_axes = 16;
    f->axes = (cp_axes**)calloc(f->cap_axes, sizeof(cp_axes*));
    return f;
}

static cp_axes *cp_subplot(cp_fig *fig, int nrows, int ncols, int index) {
    if (fig->n_axes >= fig->cap_axes) {
        fig->cap_axes *= 2;
        fig->axes = (cp_axes**)realloc(fig->axes, fig->cap_axes * sizeof(cp_axes*));
    }
    cp_axes *ax = (cp_axes*)calloc(1, sizeof(cp_axes));
    ax->auto_x = 1;
    ax->auto_y = 1;
    ax->subplot_row = index / ncols;
    ax->subplot_col = index % ncols;

    int top_offset = fig->suptitle_str[0] ? CP__SUP_H : 0;
    int avail_w = fig->width;
    int avail_h = fig->height - top_offset;
    int cell_w = avail_w / ncols;
    int cell_h = avail_h / nrows;

    int cx = ax->subplot_col * cell_w;
    int cy = top_offset + ax->subplot_row * cell_h;

    ax->px = cx + CP__MARGIN_L;
    ax->py = cy + CP__MARGIN_T;
    ax->pw = cell_w - CP__MARGIN_L - CP__MARGIN_R;
    ax->ph = cell_h - CP__MARGIN_T - CP__MARGIN_B;
    if (ax->pw < 10) ax->pw = 10;
    if (ax->ph < 10) ax->ph = 10;

    fig->axes[fig->n_axes++] = ax;
    return ax;
}

static void cp_suptitle(cp_fig *fig, const char *text) {
    strncpy(fig->suptitle_str, text, sizeof(fig->suptitle_str)-1);
    /* recalculate axes positions */
    for (int i = 0; i < fig->n_axes; i++) {
        cp_axes *ax = fig->axes[i];
        /* simple: just push everything down if suptitle added after subplot */
        (void)ax; /* positions set in subplot, re-call subplot if needed */
    }
}

static void cp_plot(cp_axes *ax, const double *x, const double *y, int n, const char *opts) {
    cp__opts o = cp__parse_opts(opts);
    if (o.line_style < 0 && o.marker == CP_MARKER_NONE)
        o.line_style = CP_SOLID; /* default: solid line */
    cp__new_series(ax, x, y, n, &o, CP_TYPE_LINE);
}

static void cp_scatter(cp_axes *ax, const double *x, const double *y, int n, const char *opts) {
    cp__opts o = cp__parse_opts(opts);
    if (o.marker == CP_MARKER_NONE) o.marker = CP_MARKER_CIRCLE;
    o.line_style = -1; /* no line */
    o.has_line = 0;
    cp__new_series(ax, x, y, n, &o, CP_TYPE_LINE);
}

static void cp_bar(cp_axes *ax, const double *x, const double *y, int n, const char *opts) {
    cp__opts o = cp__parse_opts(opts);
    o.line_style = -1;
    cp_series *s = cp__new_series(ax, x, y, n, &o, CP_TYPE_BAR);
    if (s) s->bar_width = o.bar_width;
}

static void cp_hist(cp_axes *ax, const double *data, int n, int bins, const char *opts) {
    if (n <= 0 || bins <= 0) return;
    double lo = data[0], hi = data[0];
    for (int i = 1; i < n; i++) {
        if (data[i] < lo) lo = data[i];
        if (data[i] > hi) hi = data[i];
    }
    if (lo == hi) { lo -= 0.5; hi += 0.5; }
    double bw = (hi - lo) / bins;
    double *cx = (double*)calloc(bins, sizeof(double));
    double *cy = (double*)calloc(bins, sizeof(double));
    for (int i = 0; i < bins; i++) cx[i] = lo + (i + 0.5) * bw;
    for (int i = 0; i < n; i++) {
        int b = (int)((data[i] - lo) / bw);
        if (b >= bins) b = bins - 1;
        if (b < 0) b = 0;
        cy[b]++;
    }
    cp__opts o = cp__parse_opts(opts);
    o.bar_width = bw * 0.92;
    o.line_style = -1;
    if (o.alpha >= 1.0) o.alpha = 0.75;
    cp__new_series(ax, cx, cy, bins, &o, CP_TYPE_BAR);
    free(cx); free(cy);
}

static void cp_hline(cp_axes *ax, double y, const char *opts) {
    cp__opts o = cp__parse_opts(opts);
    if (o.line_style < 0) o.line_style = CP_DASHED;
    o.has_line = 1;
    double xd = 0, yd = y;
    cp__new_series(ax, &xd, &yd, 1, &o, CP_TYPE_HLINE);
}

static void cp_vline(cp_axes *ax, double x, const char *opts) {
    cp__opts o = cp__parse_opts(opts);
    if (o.line_style < 0) o.line_style = CP_DASHED;
    o.has_line = 1;
    double xd = x, yd = 0;
    cp__new_series(ax, &xd, &yd, 1, &o, CP_TYPE_VLINE);
}

static void cp_fill_between(cp_axes *ax, const double *x,
                              const double *y1, const double *y2, int n, const char *opts)
{
    cp__opts o = cp__parse_opts(opts);
    if (o.alpha >= 1.0) o.alpha = 0.3;
    o.line_style = -1;
    /* store y1 and y2 concatenated in y array */
    double *y_both = (double*)malloc(2*n*sizeof(double));
    memcpy(y_both, y1, n*sizeof(double));
    memcpy(y_both+n, y2, n*sizeof(double));
    /* expand range for both y1 and y2 */
    for (int i=0; i<n; i++) {
        double lo = y1[i] < y2[i] ? y1[i] : y2[i];
        double hi = y1[i] > y2[i] ? y1[i] : y2[i];
        if (ax->n_series==0 && i==0 && ax->auto_y) { ax->ymin=lo; ax->ymax=hi; }
        if (lo < ax->ymin) ax->ymin = lo;
        if (hi > ax->ymax) ax->ymax = hi;
    }
    cp_series *s = (cp_series*)calloc(1, sizeof(cp_series));
    s->x = (double*)malloc(n*sizeof(double)); memcpy(s->x, x, n*sizeof(double));
    s->y = y_both;
    s->n = 2*n;
    s->type = CP_TYPE_FILL;
    s->color = o.has_color ? o.color : cp__palette[ax->n_series % CP__NPALETTE];
    s->fill_alpha = o.alpha;
    s->line_style = -1;
    if (o.label[0]) strncpy(s->label, o.label, sizeof(s->label)-1);
    /* update x range */
    for (int i=0;i<n;i++) {
        if (ax->n_series==0&&i==0&&ax->auto_x){ax->xmin=ax->xmax=x[i];}
        if (x[i]<ax->xmin)ax->xmin=x[i]; if(x[i]>ax->xmax)ax->xmax=x[i];
    }
    s->next=NULL;
    if (!ax->series_head) ax->series_head=s;
    else { cp_series*t=ax->series_head; while(t->next)t=t->next; t->next=s; }
    ax->n_series++;
}

static void cp_title(cp_axes *ax, const char *text) {
    strncpy(ax->title_str, text, sizeof(ax->title_str)-1);
}
static void cp_xlabel(cp_axes *ax, const char *text) {
    strncpy(ax->xlabel_str, text, sizeof(ax->xlabel_str)-1);
}
static void cp_ylabel(cp_axes *ax, const char *text) {
    strncpy(ax->ylabel_str, text, sizeof(ax->ylabel_str)-1);
}
static void cp_xlim(cp_axes *ax, double lo, double hi) {
    ax->xmin = lo; ax->xmax = hi; ax->auto_x = 0;
}
static void cp_ylim(cp_axes *ax, double lo, double hi) {
    ax->ymin = lo; ax->ymax = hi; ax->auto_y = 0;
}
static void cp_grid(cp_axes *ax, int on) { ax->show_grid = on; }
static void cp_legend(cp_axes *ax) { ax->show_legend = 1; }

static int cp_save(cp_fig *fig, const char *filename) {
    cp__render(fig);
    int len = (int)strlen(filename);
    if (len > 4 && strcmp(filename+len-4, ".tga") == 0)
        return cp__write_tga(filename, fig->pixels, fig->width, fig->height);
    return cp__write_bmp(filename, fig->pixels, fig->width, fig->height);
}

static void cp_free(cp_fig *fig) {
    if (!fig) return;
    for (int i = 0; i < fig->n_axes; i++) {
        cp_axes *ax = fig->axes[i];
        cp_series *s = ax->series_head;
        while (s) {
            cp_series *next = s->next;
            free(s->x); free(s->y); free(s);
            s = next;
        }
        free(ax);
    }
    free(fig->axes);
    free(fig->pixels);
    free(fig);
}

#endif /* CPLOT_IMPLEMENTATION */
#endif /* CPLOT_H */

