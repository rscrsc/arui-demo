// SPDX-License-Identifier: 0BSD
// Copyright (c) 2026 Arcie Ren

/**
 * pit.js v0.1.0
 * 
 * Reactive Web Components for Unix minds.
 * 
 * signal(v)           ≈ inotify watched file
 * computed(fn)        ≈ /proc virtual file
 * effect(fn)          ≈ epoll callback
 * component(tag, fn)  ≈ isolated process
 * store(url, init)    ≈ pipe from server
 * select(sig, fn)     ≈ /proc/cpuinfo (projection)
 * router(fallback)    ≈ virtual console switch
 * bus                 ≈ AF_UNIX broadcast
 *
 * Zero dependencies. Recommend lit-html.js for templates.
 * No build step. Copy this file into your project.
 */

// ================================================================
//  REACTIVE CORE
// ================================================================

let _active = null;      // currently executing effect (ptrace target)
let _pending = new Set(); // dirty effects awaiting execution
let _scheduled = false;   // microtask queued?

/** @internal Remove effect from all its signal subscriber lists */
function _untrack(eff) {
    for (const subs of eff._d) subs.delete(eff);
    eff._d.clear();
}

/** @internal Register current active effect as subscriber */
function _track(subs) {
    if (_active) {
        subs.add(_active);
        _active._d.add(subs);   // effect remembers which sub-lists it's in
    }
}

/** @internal Schedule subscribers for re-execution */
function _notify(subs) {
    for (const eff of subs) _pending.add(eff);
    if (!_scheduled) {
        _scheduled = true;
        queueMicrotask(_flush);
    }
}

/** @internal Batch-execute all dirty effects */
function _flush() {
    let rounds = 0;
    while (_pending.size > 0 && rounds++ < 10) {
        const batch = [..._pending];
        _pending.clear();
        for (const eff of batch) {
            if (!eff._x) eff();
        }
    }
    _scheduled = false;
    if (_pending.size > 0) {
        console.error('[pit] reactive cycle did not converge — forcing stop');
        _pending.clear();
    }
}

/**
 * Create an observable value.
 *   const count = signal(0);
 *   count.get()    — read (tracks caller if inside effect)
 *   count.set(1)   — write (notifies all subscribers)
 *   count.peek()   — read without tracking
 *   count.update(v => v + 1) — read-modify-write
 */
export function signal(value) {
    const subs = new Set();
    return {
        get()       { _track(subs); return value; },
        set(v)      { if (!Object.is(v, value)) { value = v; _notify(subs); } },
        peek()      { return value; },
        update(fn)  { this.set(fn(value)); }
    };
}

/**
 * Create an auto-rerunning side effect.
 * Returns a dispose function.
 *   const stop = effect(() => console.log(count.get()));
 *   stop(); // unsubscribe
 */
export function effect(fn) {
    function eff() {
        _untrack(eff);
        const prev = _active;
        _active = eff;
        try { fn(); } finally { _active = prev; }
    }
    eff._d = new Set();    // dependency sub-lists
    eff._x = false;        // disposed flag
    eff();                  // synchronous first run (collects deps)
    return () => { eff._x = true; _untrack(eff); };
}

/**
 * Create a derived (read-only) value.
 *   const doubled = computed(() => count.get() * 2);
 *   doubled.get()   — read derived value
 *   doubled.dispose() — stop computing
 */
export function computed(fn) {
    let val;
    const subs = new Set();
    const dispose = effect(() => {
        const next = fn();
        if (!Object.is(next, val)) {
            val = next;
            _notify(subs);
        }
    });
    return {
        get()  { _track(subs); return val; },
        peek() { return val; },
        dispose
    };
}

// ================================================================
//  SHALLOW EQUALITY (for select / state slicing)
// ================================================================

function _shallowEq(a, b) {
    if (Object.is(a, b)) return true;
    if (!a || !b || typeof a !== 'object' || typeof b !== 'object') return false;
    if (Array.isArray(a)) {
        return Array.isArray(b) && a.length === b.length &&
               a.every((v, i) => Object.is(v, b[i]));
    }
    const ka = Object.keys(a);
    return ka.length === Object.keys(b).length &&
           ka.every(k => Object.is(a[k], b[k]));
}

/**
 * Create a shallow-compared derived value from another signal.
 * Prevents downstream effects from firing when the slice hasn't really changed.
 *   const items = select(serverState, s => s.items);
 */
export function select(source, fn) {
    let val;
    const subs = new Set();
    const dispose = effect(() => {
        const next = fn(source.get());
        if (!_shallowEq(next, val)) {
            val = next;
            _notify(subs);
        }
    });
    return {
        get()  { _track(subs); return val; },
        peek() { return val; },
        dispose
    };
}

// ================================================================
//  WEB COMPONENT HELPER
// ================================================================

/**
 * Define a Web Component.
 * 
 *   component('my-chart', (ctx) => {
 *       ctx.root.innerHTML = `<canvas></canvas>`;
 *       const canvas = ctx.$('canvas');
 *       ctx.effect(() => draw(canvas, state.get()));
 *       ctx.on(canvas, 'click', (e) => ctx.emit('select', e));
 *       ctx.css`canvas { border: 1px solid #333; }`;
 *   });
 */
export function component(tag, setup) {
    class W extends HTMLElement {
        connectedCallback() {
            if (this._w) return;    // already mounted
            this._w = [];           // cleanup functions

            this.attachShadow({ mode: 'open' });

            const self = this;
            const ctx = {
                // ---- DOM access ----
                host: self,
                root: self.shadowRoot,
                $(sel)  { return self.shadowRoot.querySelector(sel); },
                $$(sel) { return [...self.shadowRoot.querySelectorAll(sel)]; },

                // ---- Attribute reading ----
                attr(name, fallback) {
                    return self.getAttribute(name) ?? fallback;
                },
                attrNum(name, fallback) {
                    const v = self.getAttribute(name);
                    return v != null ? Number(v) : fallback;
                },

                // ---- Lifecycle-bound effect ----
                effect(fn) {
                    const dispose = effect(fn);
                    self._w.push(dispose);
                    return dispose;
                },

                // ---- Lifecycle-bound event listener ----
                on(target, event, handler, opts) {
                    target.addEventListener(event, handler, opts);
                    self._w.push(() =>
                        target.removeEventListener(event, handler, opts)
                    );
                },

                // ---- Emit custom event (crosses Shadow DOM boundary) ----
                emit(name, detail) {
                    self.dispatchEvent(new CustomEvent(name, {
                        detail, bubbles: true, composed: true
                    }));
                },

                // ---- Add scoped CSS ----
                // NOTE: remember to do at last to avoid overriding
                css(strings, ...values) {
                    const style = document.createElement('style');
                    style.textContent = typeof strings === 'string'
                        ? strings
                        : String.raw({ raw: strings }, ...values);
                    self.shadowRoot.prepend(style);
                },

                // ---- Manual cleanup registration ----
                cleanup(fn) { self._w.push(fn); }
            };

            setup(ctx);
        }

        disconnectedCallback() {
            if (!this._w) return;
            for (const fn of this._w) fn();
            this._w = null;
        }
    }

    customElements.define(tag, W);
    return W;
}

// ================================================================
//  STORE (WebSocket + Signal)
// ================================================================

/**
 * Create a WebSocket-backed reactive store.
 *   const app = store(`ws://${location.host}/ws`);
 *   app.state.get()           — current server state
 *   app.connected.get()       — connection status
 *   app.send({ type: 'run' }) — send action to server
 *   app.select(s => s.chart)  — derived slice (shallow-compared)
 */
export function store(url, initial = {}) {
    const state     = signal(initial);
    const connected = signal(false);
    let ws, timer;

    function connect() {
        ws = new WebSocket(url);
        ws.onopen  = () => connected.set(true);
        ws.onerror = (e) => console.error('[pit] ws error:', e);
        ws.onclose = () => {
            connected.set(false);
            timer = setTimeout(connect, 2000);  // auto reconnect
        };
        ws.onmessage = (e) => {
            try {
                state.set(JSON.parse(e.data));
            } catch (err) {
                console.error('[pit] bad JSON from server:', err);
            }
        };
    }

    connect();

    return {
        state,
        connected,
        send(action) {
            if (ws?.readyState === WebSocket.OPEN) {
                ws.send(typeof action === 'string'
                    ? action
                    : JSON.stringify(action));
            }
        },
        select(fn) {
            return select(state, fn);
        },
        close() {
            clearTimeout(timer);
            ws?.close();
        }
    };
}

// ================================================================
//  ROUTER (hash-based)
// ================================================================

/**
 * Hash-based router.
 *   const nav = router('dashboard');
 *   nav.route.get()       — current route string
 *   nav.go('settings')    — navigate
 *   nav.is('settings')    — computed boolean
 */
export function router(fallback = '') {
    const route = signal(location.hash.slice(1) || fallback);

    window.addEventListener('hashchange', () => {
        route.set(location.hash.slice(1) || fallback);
    });

    return {
        route,
        go(path) { location.hash = path; },
        is(path) { return computed(() => route.get() === path); }
    };
}

// ================================================================
//  EVENT BUS (cross-component client-side messaging)
// ================================================================

const _bus = new EventTarget();

/** Global event bus — for client-side-only cross-component communication */
export const bus = {
    emit(name, detail) {
        _bus.dispatchEvent(new CustomEvent(name, { detail }));
    },
    on(name, handler) {
        _bus.addEventListener(name, handler);
        return () => _bus.removeEventListener(name, handler);
    }
};
