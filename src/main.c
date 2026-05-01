// SPDX-License-Identifier: 0BSD
// Copyright (c) 2026 Arcie Ren

#define CPLOT_IMPLEMENTATION
#include <cplot.h>
#include <jr.h>
#include <jw.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#define MAX_PTS 10000

typedef struct { double x, y; } pt;

typedef struct {
    pt pts[MAX_PTS];
    int n;
    double avg, min, max;
} state;

state g_state = {0};

static void calc_stats(state *s) {
    if (s->n == 0) { s->avg = s->min = s->max = 0; return; }
    double sum = 0;
    s->min = s->max = fabs(s->pts[0].y);
    for (int i = 0; i < s->n; i++) {
        double v = fabs(s->pts[i].y);
        sum += v;
        if (v < s->min) s->min = v;
        if (v > s->max) s->max = v;
    }
    s->avg = sum / s->n;
}

static void send_state(void) {
    jw j = jw_to(stdout);
    jw_begin(&j);
    jw_key(&j, "v");
    jw_arr(&j);
    for (int i = 0; i < g_state.n; i++) {
        jw_begin(&j);
        jw_kv_double(&j, "x", g_state.pts[i].x);
        jw_kv_double(&j, "y", g_state.pts[i].y);
        jw_end(&j);
    }
    jw_end(&j);
    jw_key(&j, "stats");
    jw_begin(&j);
    jw_kv_double(&j, "avg", g_state.avg);
    jw_kv_double(&j, "min", g_state.min);
    jw_kv_double(&j, "max", g_state.max);
    jw_end(&j);
    jw_kv_int(&j, "max_pts", MAX_PTS);
    jw_end(&j);
    jw_endl(&j);
}

static void add_point(double x, double y) {
    if (g_state.n >= MAX_PTS) return;
    g_state.pts[g_state.n++] = (pt){x, y};
    calc_stats(&g_state);
    send_state();
}

static void gen_noise(int n) {
    if (g_state.n == 0) {
        // No existing points, start at origin
        for (int i = 0; i < n && g_state.n < MAX_PTS; i++) {
            g_state.pts[g_state.n++] = (pt){i * 0.1, 0};
        }
        calc_stats(&g_state);
        send_state();
        return;
    }

    if (g_state.n == 1) {
        // Only one point, extend in X direction
        double lastX = g_state.pts[0].x;
        double lastY = g_state.pts[0].y;
        for (int i = 1; i <= n && g_state.n < MAX_PTS; i++) {
            g_state.pts[g_state.n++] = (pt){lastX + i * 0.1, lastY};
        }
        calc_stats(&g_state);
        send_state();
        return;
    }

    // Find current X range
    double xMin = g_state.pts[0].x, xMax = g_state.pts[0].x;
    for (int i = 1; i < g_state.n; i++) {
        if (g_state.pts[i].x < xMin) xMin = g_state.pts[i].x;
        if (g_state.pts[i].x > xMax) xMax = g_state.pts[i].x;
    }
    double xRange = xMax - xMin;

    // Direction from last section
    pt last = g_state.pts[g_state.n - 1];
    pt prev = g_state.pts[g_state.n - 2];
    double dirX = last.x - prev.x;
    double dirY = last.y - prev.y;

    // Normalize direction to unit vector
    double dirLen = sqrt(dirX * dirX + dirY * dirY);
    if (dirLen > 0) {
        dirX /= dirLen;
        dirY /= dirLen;
    } else {
        dirX = 1; dirY = 0; // default to X direction
    }

    // Total X extension: 30% of current range
    double xExtension = xRange * 0.3;

    // X spacing for new points
    double xSpacing = xExtension / n;

    // Noise strength: golden ratio / 2 * xSpacing ≈ 0.809 * xSpacing
    double noiseStrength = 0.809 * xSpacing;

    // Generate points
    for (int i = 1; i <= n && g_state.n < MAX_PTS; i++) {
        double t = (double)i / n;
        double newX = last.x + dirX * xExtension * t;
        // Base Y follows direction, with perpendicular noise
        double baseY = last.y + dirY * xExtension * t;
        // Perpendicular direction for noise
        double perpX = -dirY;
        double perpY = dirX;
        // Random noise component
        double noise = ((double)rand() / RAND_MAX - 0.5) * noiseStrength;
        double newY = baseY + perpY * noise;

        g_state.pts[g_state.n++] = (pt){newX, newY};
    }

    calc_stats(&g_state);
    send_state();
}

static void gen_plot(void) {
    if (g_state.n < 2) return;
    double x[MAX_PTS], y[MAX_PTS];
    for (int i = 0; i < g_state.n; i++) {
        x[i] = g_state.pts[i].x;
        y[i] = g_state.pts[i].y;
    }
    cp_fig *f = cp_figure(640, 400);
    cp_axes *ax = cp_subplot(f, 1, 1, 0);
    cp_plot(ax, x, y, g_state.n, "b-|label=data|lw=2");
    cp_hline(ax, 0, "r--|lw=1");
    cp_title(ax, "Data Plot");
    cp_xlabel(ax, "x"); cp_ylabel(ax, "y");
    cp_grid(ax, 1); cp_legend(ax);
    cp_save(f, "plot.bmp");
    cp_free(f);

    jw j = jw_to(stdout);
    jw_begin(&j);
    jw_kv_str(&j, "type", "plot_success");
    jw_kv_str(&j, "file", "plot.bmp");
    jw_end(&j);
    jw_endl(&j);

    fprintf(stderr, "plot saved to plot.bmp\n");
}

static void del_point(int idx) {
    if (idx < 0 || idx >= g_state.n) return;
    for (int i = idx; i < g_state.n - 1; i++) {
        g_state.pts[i] = g_state.pts[i + 1];
    }
    g_state.n--;
    calc_stats(&g_state);
    send_state();
}

static void handle_msg(const char *json) {
    jr r = jr_new(json);
    jr_open(&r);
    while (jr_key(&r)) {
        if (jr_eq(&r, "type")) {
            char type[16] = {0};
            jr_val_str(&r, type, sizeof(type));
            if (strcmp(type, "start") == 0) {
                send_state();
            } else if (strcmp(type, "reset") == 0) {
                g_state.n = 0;
                calc_stats(&g_state);
                send_state();
            } else if (strcmp(type, "add") == 0) {
                double x = 0, y = 0;
                while (jr_key(&r)) {
                    if (jr_eq(&r, "x")) x = jr_val_double(&r);
                    else if (jr_eq(&r, "y")) y = jr_val_double(&r);
                    else jr_val_skip(&r);
                }
                add_point(x, y);
            } else if (strcmp(type, "gen") == 0) {
                int n = 10;
                while (jr_key(&r)) {
                    if (jr_eq(&r, "n")) n = jr_val_int(&r);
                    else jr_val_skip(&r);
                }
                gen_noise(n);
            } else if (strcmp(type, "plot") == 0) {
                gen_plot();
            } else if (strcmp(type, "del") == 0) {
                int idx = -1;
                while (jr_key(&r)) {
                    if (jr_eq(&r, "idx")) idx = jr_val_int(&r);
                    else jr_val_skip(&r);
                }
                del_point(idx);
            }
        } else {
            jr_val_skip(&r);
        }
    }
}

int main(void) {
    srand((unsigned)time(NULL));
    char buf[4096];
    while (fgets(buf, sizeof(buf), stdin)) {
        size_t len = strlen(buf);
        if (len > 0 && buf[len-1] == '\n') buf[len-1] = 0;
        if (len > 0 && buf[len-2] == '\r') buf[len-2] = 0;
        handle_msg(buf);
    }
    return 0;
}
