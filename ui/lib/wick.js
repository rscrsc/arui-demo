// SPDX-License-Identifier: 0BSD
// Copyright (c) 2026 Arcie Ren

// wick.js — olive.c-style canvas helpers for pit.js
// Usage:
//   import { wick, clamp, lerp, hsl, dist } from './lib/wick.js';
//   const w = wick(ctx, { width: 600, height: 400 });
//   w.draw((dt, frame) => { w.fill('#111'); w.circle(w.mouse.x, w.mouse.y, 10, '#f00'); });

// ══════════════════════════════════════════════════════════
//  Core factory
// ══════════════════════════════════════════════════════════

export function wick(ctx, opts = {}) {
    const width  = opts.width  ?? 600;
    const height = opts.height ?? 400;
    const dpr    = opts.dpr    ?? (window.devicePixelRatio || 1);

    // ── Canvas element ──────────────────────────────────
    const el = document.createElement('canvas');
    el.width  = width * dpr;
    el.height = height * dpr;
    el.style.width   = width + 'px';
    el.style.height  = height + 'px';
    el.style.display = 'block';
    el.style.cursor  = 'crosshair';

    const c = el.getContext('2d');
    c.scale(dpr, dpr);

    ctx.root.appendChild(el);

    // ── Mouse state (no signals — read directly each frame, zero overhead) ──
    let mx = 0, my = 0, mdown = false, mBtn = 0;
    let mdownThisFrame = false, mupThisFrame = false;
    let scrollDelta = 0;

    el.addEventListener('mousemove', e => { mx = e.offsetX; my = e.offsetY; });
    el.addEventListener('mousedown', e => {
        mdown = true; mdownThisFrame = true; mBtn = e.button;
        mx = e.offsetX; my = e.offsetY;
    });
    el.addEventListener('mouseup',    () => { mdown = false; mupThisFrame = true; });
    el.addEventListener('mouseleave', () => { mdown = false; });
    el.addEventListener('contextmenu', e => e.preventDefault());
    el.addEventListener('wheel', e => { scrollDelta += e.deltaY; e.preventDefault(); }, { passive: false });

    // ── Touch → mouse emulation ─────────────────────────
    function touchXY(e) {
        const r = el.getBoundingClientRect();
        const t = e.touches[0];
        if (t) { mx = t.clientX - r.left; my = t.clientY - r.top; }
    }
    el.addEventListener('touchstart', e => { mdown = true; mdownThisFrame = true; touchXY(e); e.preventDefault(); }, { passive: false });
    el.addEventListener('touchmove',  e => { touchXY(e); e.preventDefault(); }, { passive: false });
    el.addEventListener('touchend',   () => { mdown = false; mupThisFrame = true; });

    // ── Keyboard state ──────────────────────────────────
    const keysDown    = new Set();
    const keysPressed = new Set();   // pressed this frame
    el.tabIndex = 0;
    el.addEventListener('keydown', e => { keysDown.add(e.key); keysPressed.add(e.key); });
    el.addEventListener('keyup',   e => { keysDown.delete(e.key); });

    // ── rAF loop ────────────────────────────────────────
    let drawFn  = null;
    let running  = true;
    let rafId    = 0;
    let prev     = 0;
    let frames   = 0;
    let fps      = 0;
    let fpsAcc   = 0;
    let fpsT     = 0;

    function loop(now) {
        if (!running) return;
        if (prev === 0) prev = now;
        const dt = Math.min((now - prev) / 1000, 0.1);   // cap at 100ms
        prev = now;
        frames++;

        fpsAcc++;
        if (now - fpsT >= 1000) { fps = fpsAcc; fpsAcc = 0; fpsT = now; }

        if (drawFn) drawFn(dt, frames);

        // clear per-frame transient input
        mdownThisFrame = false;
        mupThisFrame   = false;
        scrollDelta    = 0;
        keysPressed.clear();

        rafId = requestAnimationFrame(loop);
    }

    // auto-cleanup when pit.js component is destroyed
    ctx.cleanup(() => { running = false; cancelAnimationFrame(rafId); });

    // ══════════════════════════════════════════════════════
    //  PUBLIC API
    // ══════════════════════════════════════════════════════

    const api = {
        el, c,
        w: width,
        h: height,

        // ── Input state ─────────────────────────────────
        get mouse() {
            return {
                x: mx, y: my,
                down: mdown, button: mBtn,
                pressed:  mdownThisFrame,   // true only on the frame mouse went down
                released: mupThisFrame,     // true only on the frame mouse went up
                scroll:   scrollDelta,
            };
        },
        get fps() { return fps; },

        key(k)        { return keysDown.has(k); },         // held
        keyPressed(k) { return keysPressed.has(k); },      // just pressed this frame

        // ── Start loop ──────────────────────────────────
        draw(fn) {
            drawFn = fn;
            prev = 0;
            fpsT = performance.now();
            rafId = requestAnimationFrame(loop);
        },

        // ── Fill entire canvas ──────────────────────────
        fill(color) {
            c.fillStyle = color;
            c.fillRect(0, 0, api.w, api.h);
        },

        // ── Rectangle ───────────────────────────────────
        rect(x, y, rw, rh, color) {
            c.fillStyle = color;
            c.fillRect(x, y, rw, rh);
        },

        frame(x, y, rw, rh, color, lw = 1) {
            c.strokeStyle = color;
            c.lineWidth = lw;
            c.strokeRect(x + 0.5, y + 0.5, rw, rh);
        },

        roundRect(x, y, rw, rh, color, radius = 4) {
            c.fillStyle = color;
            c.beginPath();
            c.roundRect(x, y, rw, rh, radius);
            c.fill();
        },

        roundFrame(x, y, rw, rh, color, radius = 4, lw = 1) {
            c.strokeStyle = color;
            c.lineWidth = lw;
            c.beginPath();
            c.roundRect(x + 0.5, y + 0.5, rw, rh, radius);
            c.stroke();
        },

        // ── Circle ──────────────────────────────────────
        circle(cx, cy, r, color) {
            c.fillStyle = color;
            c.beginPath();
            c.arc(cx, cy, Math.abs(r), 0, Math.PI * 2);
            c.fill();
        },

        ring(cx, cy, r, color, lw = 1) {
            c.strokeStyle = color;
            c.lineWidth = lw;
            c.beginPath();
            c.arc(cx, cy, Math.abs(r), 0, Math.PI * 2);
            c.stroke();
        },

        // ── Arc ─────────────────────────────────────────
        arc(cx, cy, r, start, end, color, lw = 1) {
            c.strokeStyle = color;
            c.lineWidth = lw;
            c.beginPath();
            c.arc(cx, cy, Math.abs(r), start, end);
            c.stroke();
        },

        wedge(cx, cy, r, start, end, color) {
            c.fillStyle = color;
            c.beginPath();
            c.moveTo(cx, cy);
            c.arc(cx, cy, Math.abs(r), start, end);
            c.closePath();
            c.fill();
        },

        // ── Line ────────────────────────────────────────
        line(x1, y1, x2, y2, color, lw = 1) {
            c.strokeStyle = color;
            c.lineWidth = lw;
            c.beginPath();
            c.moveTo(x1, y1);
            c.lineTo(x2, y2);
            c.stroke();
        },

        // ── Polyline / Polygon ──────────────────────────
        poly(pts, color, lw = 1, close = false) {
            if (pts.length < 2) return;
            c.strokeStyle = color;
            c.lineWidth = lw;
            c.beginPath();
            c.moveTo(pts[0][0], pts[0][1]);
            for (let i = 1; i < pts.length; i++) c.lineTo(pts[i][0], pts[i][1]);
            if (close) c.closePath();
            c.stroke();
        },

        polyFill(pts, color) {
            if (pts.length < 3) return;
            c.fillStyle = color;
            c.beginPath();
            c.moveTo(pts[0][0], pts[0][1]);
            for (let i = 1; i < pts.length; i++) c.lineTo(pts[i][0], pts[i][1]);
            c.closePath();
            c.fill();
        },

        // ── Triangle ────────────────────────────────────
        triangle(x1, y1, x2, y2, x3, y3, color) {
            c.fillStyle = color;
            c.beginPath();
            c.moveTo(x1, y1);
            c.lineTo(x2, y2);
            c.lineTo(x3, y3);
            c.closePath();
            c.fill();
        },

        // ── Text ────────────────────────────────────────
        text(str, x, y, color, size = 14, font = 'monospace') {
            c.fillStyle = color;
            c.font = `${size}px ${font}`;
            c.textAlign = 'start';
            c.textBaseline = 'top';
            c.fillText(str, x, y);
        },

        textCenter(str, x, y, color, size = 14, font = 'monospace') {
            c.fillStyle = color;
            c.font = `${size}px ${font}`;
            c.textAlign = 'center';
            c.textBaseline = 'middle';
            c.fillText(str, x, y);
            c.textAlign = 'start';
            c.textBaseline = 'top';
        },

        textRight(str, x, y, color, size = 14, font = 'monospace') {
            c.fillStyle = color;
            c.font = `${size}px ${font}`;
            c.textAlign = 'right';
            c.textBaseline = 'top';
            c.fillText(str, x, y);
            c.textAlign = 'start';
        },

        measure(str, size = 14, font = 'monospace') {
            c.font = `${size}px ${font}`;
            return c.measureText(str).width;
        },

        // ── Image ───────────────────────────────────────
        img(image, x, y, iw, ih) {
            if (iw !== undefined) c.drawImage(image, x, y, iw, ih);
            else c.drawImage(image, x, y);
        },

        sprite(image, sx, sy, sw, sh, dx, dy, dw, dh) {
            c.drawImage(image, sx, sy, sw, sh, dx, dy, dw, dh);
        },

        // ── Gradient ────────────────────────────────────
        linearGrad(x1, y1, x2, y2, stops) {
            const g = c.createLinearGradient(x1, y1, x2, y2);
            for (const [offset, color] of stops) g.addColorStop(offset, color);
            return g;
        },

        radialGrad(cx, cy, r, stops) {
            const g = c.createRadialGradient(cx, cy, 0, cx, cy, r);
            for (const [offset, color] of stops) g.addColorStop(offset, color);
            return g;
        },

        // ── Transform ───────────────────────────────────
        save()          { c.save(); },
        restore()       { c.restore(); },
        translate(x, y) { c.translate(x, y); },
        rotate(a)       { c.rotate(a); },
        scale(sx, sy)   { c.scale(sx, sy ?? sx); },
        alpha(a)        { c.globalAlpha = a; },
        blend(mode)     { c.globalCompositeOperation = mode; },

        clip(x, y, cw, ch) {
            c.save();
            c.beginPath();
            c.rect(x, y, cw, ch);
            c.clip();
        },

        unclip() { c.restore(); },

        // ── Pixel access (olive.c buffer style) ─────────
        pixels() {
            return c.getImageData(0, 0, el.width, el.height);
        },

        putPixels(imageData, x = 0, y = 0) {
            c.putImageData(imageData, x, y);
        },

        // ── Shadow ──────────────────────────────────────
        shadow(color, blur, ox = 0, oy = 0) {
            c.shadowColor   = color;
            c.shadowBlur    = blur;
            c.shadowOffsetX = ox;
            c.shadowOffsetY = oy;
        },

        noShadow() {
            c.shadowColor   = 'transparent';
            c.shadowBlur    = 0;
            c.shadowOffsetX = 0;
            c.shadowOffsetY = 0;
        },

        // ── Utility ─────────────────────────────────────
        clear() {
            c.clearRect(0, 0, api.w, api.h);
        },

        resize(nw, nh) {
            api.w = nw;
            api.h = nh;
            el.width  = nw * dpr;
            el.height = nh * dpr;
            el.style.width  = nw + 'px';
            el.style.height = nh + 'px';
            c.scale(dpr, dpr);
        },

        // hit-test: is point inside rectangle?
        hit(px, py, rx, ry, rw, rh) {
            return px >= rx && px <= rx + rw && py >= ry && py <= ry + rh;
        },

        // hit-test: is point inside circle?
        hitCircle(px, py, cx, cy, r) {
            return (px - cx) ** 2 + (py - cy) ** 2 <= r * r;
        },
    };

    return api;
}

// ══════════════════════════════════════════════════════════
//  Math utilities
// ══════════════════════════════════════════════════════════

export const clamp   = (v, lo, hi) => v < lo ? lo : v > hi ? hi : v;
export const lerp    = (a, b, t)   => a + (b - a) * t;
export const invLerp = (a, b, v)   => (v - a) / (b - a);
export const map     = (v, a1, b1, a2, b2) => a2 + (v - a1) / (b1 - a1) * (b2 - a2);
export const dist    = (x1, y1, x2, y2) => Math.hypot(x2 - x1, y2 - y1);
export const distSq  = (x1, y1, x2, y2) => (x2 - x1) ** 2 + (y2 - y1) ** 2;
export const angle   = (x1, y1, x2, y2) => Math.atan2(y2 - y1, x2 - x1);
export const normalize = (x, y) => { const d = Math.hypot(x, y) || 1; return [x / d, y / d]; };

// ── Color helpers ───────────────────────────────────────
export const hsl     = (h, s = 70, l = 60)    => `hsl(${h},${s}%,${l}%)`;
export const hsla    = (h, s, l, a)            => `hsla(${h},${s}%,${l}%,${a})`;
export const rgba    = (r, g, b, a = 1)        => `rgba(${r},${g},${b},${a})`;
export const gray    = (v, a = 1)              => `rgba(${v},${v},${v},${a})`;
export const hex     = (n) => '#' + (n >>> 0).toString(16).padStart(6, '0');

// ── Random ──────────────────────────────────────────────
export const rng     = (lo, hi) => lo + Math.random() * (hi - lo);
export const rngInt  = (lo, hi) => Math.floor(rng(lo, hi));
export const pick    = (arr)    => arr[(Math.random() * arr.length) | 0];
export const shuffle = (arr)    => { for (let i = arr.length - 1; i > 0; i--) { const j = (Math.random() * (i + 1)) | 0; [arr[i], arr[j]] = [arr[j], arr[i]]; } return arr; };

// ── Easing ──────────────────────────────────────────────
export const ease = {
    linear:    t => t,
    inQuad:    t => t * t,
    outQuad:   t => t * (2 - t),
    inOutQuad: t => t < 0.5 ? 2 * t * t : -1 + (4 - 2 * t) * t,
    inCubic:   t => t * t * t,
    outCubic:  t => (--t) * t * t + 1,
    inOutCubic:t => t < 0.5 ? 4 * t * t * t : (t - 1) * (2 * t - 2) * (2 * t - 2) + 1,
    inSine:    t => 1 - Math.cos(t * Math.PI / 2),
    outSine:   t => Math.sin(t * Math.PI / 2),
    inOutSine: t => -(Math.cos(Math.PI * t) - 1) / 2,
    inExpo:    t => t === 0 ? 0 : 2 ** (10 * t - 10),
    outExpo:   t => t === 1 ? 1 : 1 - 2 ** (-10 * t),
    inBack:    t => 2.70158 * t * t * t - 1.70158 * t * t,
    outBack:   t => { t--; return 1 + 2.70158 * t * t * t + 1.70158 * t * t; },
    outBounce: t => {
        if (t < 1/2.75)     return 7.5625*t*t;
        if (t < 2/2.75)     return 7.5625*(t-=1.5/2.75)*t+0.75;
        if (t < 2.5/2.75)   return 7.5625*(t-=2.25/2.75)*t+0.9375;
        return 7.5625*(t-=2.625/2.75)*t+0.984375;
    },
};

// ── 2D Vector (plain arrays: [x, y]) ───────────────────
export const vec = {
    add:   (a, b)    => [a[0] + b[0], a[1] + b[1]],
    sub:   (a, b)    => [a[0] - b[0], a[1] - b[1]],
    scale: (a, s)    => [a[0] * s, a[1] * s],
    len:   (a)       => Math.hypot(a[0], a[1]),
    norm:  (a)       => { const d = Math.hypot(a[0], a[1]) || 1; return [a[0]/d, a[1]/d]; },
    dot:   (a, b)    => a[0]*b[0] + a[1]*b[1],
    cross: (a, b)    => a[0]*b[1] - a[1]*b[0],
    lerp:  (a, b, t) => [a[0]+(b[0]-a[0])*t, a[1]+(b[1]-a[1])*t],
    rot:   (a, rad)  => {
        const cos = Math.cos(rad), sin = Math.sin(rad);
        return [a[0]*cos - a[1]*sin, a[0]*sin + a[1]*cos];
    },
    dist:  (a, b)    => Math.hypot(b[0]-a[0], b[1]-a[1]),
    angle: (a, b)    => Math.atan2(b[1]-a[1], b[0]-a[0]),
};

