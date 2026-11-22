// Protocol v1 client (docs/protocol.md): hello, a ping every 3 s with pong
// timeout and reconnect backoff, and the box's welcome/pong/view/err/buzz.
// The link timings from the phase 0 field test are kept for #diag.

import type { View } from './types';

export type LogEvent = 'connected' | 'reconnected' | 'lost' | 'pongTimeout' | 'resumed';

/** Language-neutral; the UI renders it in the current language. */
export interface LogEntry {
  at: number;
  event: LogEvent;
  ms?: number;
}

export interface LinkState {
  status: 'connecting' | 'open' | 'closed';
  rttMs: number | null;
  reconnects: number;
  /** Socket lost -> next open, last occurrence. */
  lastGapMs: number | null;
  /** Page became visible -> first pong, last occurrence. */
  resumeMs: number | null;
  log: LogEntry[];
}

export interface LinkHandlers {
  state(s: LinkState): void;
  view(v: View): void;
  err(code: string): void;
  buzz(pattern: number[]): void;
}

export const initialLinkState: LinkState = {
  status: 'connecting',
  rttMs: null,
  reconnects: 0,
  lastGapMs: null,
  resumeMs: null,
  log: [],
};

const PING_EVERY_MS = 3000;
const PONG_TIMEOUT_MS = 4000;
const MAX_BACKOFF_MS = 4000;
const LOG_LINES = 30;
const ID_KEY = 'pg.clientId';

function randomHex(bytes: number): string {
  // crypto.randomUUID needs a secure context; the box is plain http.
  return Array.from(crypto.getRandomValues(new Uint8Array(bytes)), (b) => b.toString(16).padStart(2, '0')).join('');
}

/**
 * The secret player id. `?dev=<name>` gives every tab its own id (kept in
 * sessionStorage, so a reload stays the same player) to play several
 * players from one browser while developing.
 */
export function clientId(): string {
  const dev = new URLSearchParams(location.search).get('dev');
  const store = dev === null ? localStorage : sessionStorage;
  const key = dev === null ? ID_KEY : `${ID_KEY}.${dev}`;
  let id = store.getItem(key);
  if (!id) {
    const prefix = dev === null ? '' : `${dev.replace(/[^A-Za-z0-9_-]/g, '').slice(0, 10) || 'dev'}-`;
    id = prefix + randomHex(dev === null ? 8 : 4);
    store.setItem(key, id);
  }
  return id;
}

export class Link {
  state: LinkState = initialLinkState;

  private ws: WebSocket | null = null;
  private everOpened = false;
  private lostAt: number | null = null;
  private visibleAt: number | null = null;
  private backoffMs = 500;
  private lastSeq = -1;
  private retryTimer: number | undefined;
  private pingTimer: number | undefined;
  private pongTimer: number | undefined;

  constructor(
    private readonly id: string,
    private readonly on: LinkHandlers,
  ) {}

  start(): void {
    document.addEventListener('visibilitychange', this.onVisibility);
    this.pingTimer = window.setInterval(() => this.ping(), PING_EVERY_MS);
    this.connect();
  }

  stop(): void {
    document.removeEventListener('visibilitychange', this.onVisibility);
    window.clearInterval(this.pingTimer);
    window.clearTimeout(this.retryTimer);
    window.clearTimeout(this.pongTimer);
    const ws = this.ws;
    this.ws = null;
    if (ws) {
      ws.onclose = null;
      ws.onmessage = null;
      ws.close();
    }
  }

  /** Sends a request; false when the socket is not open (the UI shows the reconnect overlay then). */
  send(msg: { t: string } & Record<string, unknown>): boolean {
    const ws = this.ws;
    if (ws?.readyState !== WebSocket.OPEN) return false;
    ws.send(JSON.stringify(msg));
    return true;
  }

  private update(patch: Partial<LinkState>, event?: LogEvent, ms?: number): void {
    const log = event ? [{ at: Date.now(), event, ms }, ...this.state.log].slice(0, LOG_LINES) : this.state.log;
    this.state = { ...this.state, ...patch, log };
    this.on.state(this.state);
  }

  private connect(): void {
    window.clearTimeout(this.retryTimer);
    const ws = new WebSocket(`${location.protocol === 'https:' ? 'wss' : 'ws'}://${location.host}/ws`);
    this.ws = ws;
    this.update({ status: 'connecting' });

    ws.onopen = () => {
      this.backoffMs = 500;
      const patch: Partial<LinkState> = { status: 'open' };
      let event: LogEvent = 'connected';
      if (this.everOpened) {
        patch.reconnects = this.state.reconnects + 1;
        if (this.lostAt !== null) {
          patch.lastGapMs = Math.round(performance.now() - this.lostAt);
          event = 'reconnected';
        }
      }
      this.everOpened = true;
      this.lostAt = null;
      this.update(patch, event, patch.lastGapMs ?? undefined);
      ws.send(JSON.stringify({ t: 'hello', id: this.id }));
      this.ping();
    };

    ws.onmessage = (ev) => {
      if (this.ws !== ws) return;
      let msg: Record<string, unknown>;
      try {
        msg = JSON.parse(String(ev.data));
      } catch {
        return;
      }
      switch (msg.t) {
        case 'welcome':
          // A new session (maybe a rebooted box) may restart the sequence.
          this.lastSeq = -1;
          break;
        case 'pong':
          this.onPong(Number(msg.c));
          break;
        case 'view': {
          const seq = Number(msg.seq);
          if (seq < this.lastSeq || typeof msg.view !== 'object' || msg.view === null) return;
          this.lastSeq = seq;
          this.on.view(msg.view as View);
          break;
        }
        case 'err':
          // The box lost our hello (it restarted under an open socket): say it again.
          if (msg.code === 'hello_first') ws.send(JSON.stringify({ t: 'hello', id: this.id }));
          else this.on.err(String(msg.code));
          break;
        case 'buzz':
          if (Array.isArray(msg.p)) this.on.buzz(msg.p.map(Number));
          break;
      }
    };

    ws.onclose = () => {
      // Failed reconnect attempts close too; only a live socket going away is news.
      this.drop(ws, this.state.status === 'open' ? 'lost' : undefined, this.backoffMs);
      this.backoffMs = Math.min(this.backoffMs * 2, MAX_BACKOFF_MS);
    };
  }

  private onPong(c: number): void {
    const now = performance.now();
    window.clearTimeout(this.pongTimer);
    this.pongTimer = undefined;
    const rttMs = Math.round(now - c);
    if (this.visibleAt !== null) {
      const resumeMs = Math.round(now - this.visibleAt);
      this.visibleAt = null;
      this.update({ rttMs, resumeMs }, 'resumed', resumeMs);
    } else {
      this.update({ rttMs });
    }
  }

  // Abandons a socket without waiting for its close handshake, which can hang
  // for a long time when the box already dropped a sleeping phone.
  private drop(ws: WebSocket, event: LogEvent | undefined, retryInMs: number): void {
    if (this.ws !== ws) return;
    this.ws = null;
    window.clearTimeout(this.pongTimer);
    this.pongTimer = undefined;
    this.lostAt ??= performance.now();
    ws.onclose = null;
    ws.onmessage = null;
    ws.close();
    this.update({ status: 'closed' }, event);
    this.retryTimer = window.setTimeout(() => this.connect(), retryInMs);
  }

  private ping(): void {
    const ws = this.ws;
    if (ws?.readyState !== WebSocket.OPEN) return;
    ws.send(JSON.stringify({ t: 'ping', c: Math.round(performance.now()) }));
    this.pongTimer ??= window.setTimeout(() => {
      this.pongTimer = undefined;
      this.drop(ws, 'pongTimeout', 0);
    }, PONG_TIMEOUT_MS);
  }

  private onVisibility = (): void => {
    if (document.visibilityState !== 'visible') return;
    this.visibleAt = performance.now();
    if (this.ws?.readyState === WebSocket.OPEN) this.ping();
    else if (!this.ws) this.connect();
  };
}
