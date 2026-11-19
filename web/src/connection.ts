// Phase 0 link to the box: hello/ping/presence over /ws plus the reconnect
// timings the field test is about (see docs/phase0-field-test.md).

export interface Peer {
  id: string;
  rtt: number;
  age: number;
}

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
  peers: Peer[];
  log: LogEntry[];
}

const PING_EVERY_MS = 3000;
const PONG_TIMEOUT_MS = 4000;
const MAX_BACKOFF_MS = 4000;
const LOG_LINES = 30;

export function clientId(): string {
  const key = 'pg.clientId';
  let id = localStorage.getItem(key);
  if (!id) {
    // crypto.randomUUID needs a secure context; the box is plain http.
    const bytes = crypto.getRandomValues(new Uint8Array(8));
    id = Array.from(bytes, (b) => b.toString(16).padStart(2, '0')).join('');
    localStorage.setItem(key, id);
  }
  return id;
}

export class Link {
  state: LinkState = {
    status: 'connecting',
    rttMs: null,
    reconnects: 0,
    lastGapMs: null,
    resumeMs: null,
    peers: [],
    log: [],
  };

  private ws: WebSocket | null = null;
  private everOpened = false;
  private lostAt: number | null = null;
  private visibleAt: number | null = null;
  private backoffMs = 500;
  private retryTimer: number | undefined;
  private pingTimer: number | undefined;
  private pongTimer: number | undefined;

  constructor(
    private readonly id: string,
    private readonly onChange: (s: LinkState) => void,
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
    ws?.close();
  }

  private update(patch: Partial<LinkState>, event?: LogEvent, ms?: number): void {
    const log = event ? [{ at: Date.now(), event, ms }, ...this.state.log].slice(0, LOG_LINES) : this.state.log;
    this.state = { ...this.state, ...patch, log };
    this.onChange(this.state);
  }

  private connect(): void {
    window.clearTimeout(this.retryTimer);
    const ws = new WebSocket(`ws://${location.host}/ws`);
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
      const msg = JSON.parse(String(ev.data));
      if (msg.t === 'pong') {
        const now = performance.now();
        window.clearTimeout(this.pongTimer);
        this.pongTimer = undefined;
        const rttMs = Math.round(now - msg.c);
        if (this.visibleAt !== null) {
          const resumeMs = Math.round(now - this.visibleAt);
          this.visibleAt = null;
          this.update({ rttMs, resumeMs }, 'resumed', resumeMs);
        } else {
          this.update({ rttMs });
        }
      } else if (msg.t === 'presence') {
        this.update({ peers: msg.clients });
      }
    };

    ws.onclose = () => {
      // Failed reconnect attempts close too; only a live socket going away is news.
      this.drop(ws, this.state.status === 'open' ? 'lost' : undefined, this.backoffMs);
      this.backoffMs = Math.min(this.backoffMs * 2, MAX_BACKOFF_MS);
    };
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
    ws.close();
    this.update({ status: 'closed' }, event);
    this.retryTimer = window.setTimeout(() => this.connect(), retryInMs);
  }

  private ping(): void {
    const ws = this.ws;
    if (ws?.readyState !== WebSocket.OPEN) return;
    ws.send(JSON.stringify({ t: 'ping', c: Math.round(performance.now()), rtt: this.state.rttMs ?? -1 }));
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
