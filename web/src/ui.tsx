// Shared context and small building blocks used by every screen.

import type { ComponentChildren } from 'preact';
import { createContext } from 'preact';
import { useContext, useEffect, useState } from 'preact/hooks';
import { t, tOr } from './i18n';
import type { Player, View } from './types';

export type Send = (msg: { t: string } & Record<string, unknown>) => void;

export interface AppCtx {
  view: View;
  /** performance.now() when view.endsIn runs out; null = no timer. */
  deadline: number | null;
  send: Send;
  narrate: boolean;
  setNarrate(on: boolean): void;
}

export const Ctx = createContext<AppCtx>(null as unknown as AppCtx);

export function useApp(): AppCtx {
  return useContext(Ctx);
}

/** Profile colours, indexed by `color` 0–11. */
export const COLORS = [
  '#f87171', '#fb923c', '#facc15', '#a3e635', '#4ade80', '#2dd4bf',
  '#38bdf8', '#818cf8', '#a78bfa', '#e879f9', '#f472b6', '#d6d3d1',
];

export function colorOf(i: number | undefined): string {
  return COLORS[i ?? -1] ?? COLORS[11];
}

export function findPlayer(view: View, pid: number): Player | undefined {
  return view.players.find((p) => p.pid === pid);
}

export function nameOf(view: View, pid: number): string {
  return findPlayer(view, pid)?.name ?? t('player', { n: pid + 1 });
}

export function roleName(role: string): string {
  return tOr(`role_${role}`, role);
}

export function Avatar({ pid, small }: { pid: number; small?: boolean }) {
  const p = findPlayer(useApp().view, pid);
  return (
    <span class={small ? 'avatar small' : 'avatar'} style={{ background: colorOf(p?.color) }} aria-hidden="true">
      {p?.avatar ?? '?'}
    </span>
  );
}

/** Avatar plus name, with "(you)" for my own pid. */
export function Who({ pid, small }: { pid: number; small?: boolean }) {
  const { view } = useApp();
  return (
    <span class="who">
      <Avatar pid={pid} small={small} />
      <span class="who-name">
        {nameOf(view, pid)}
        {pid === view.me && <span class="muted"> ({t('you')})</span>}
      </span>
    </span>
  );
}

/** Seconds left on the phase timer, ticking; null when there is no timer. */
export function useSecondsLeft(): number | null {
  const { deadline } = useApp();
  const [now, setNow] = useState(() => performance.now());
  useEffect(() => {
    if (deadline === null) return;
    setNow(performance.now());
    const id = window.setInterval(() => setNow(performance.now()), 250);
    return () => window.clearInterval(id);
  }, [deadline]);
  return deadline === null ? null : Math.max(0, Math.ceil((deadline - now) / 1000));
}

export function Countdown({ big }: { big?: boolean }) {
  const s = useSecondsLeft();
  if (s === null) return null;
  return (
    <span class={big ? 'countdown big' : 'countdown'} role="timer">
      {Math.floor(s / 60)}:{String(s % 60).padStart(2, '0')}
    </span>
  );
}

/** Shows `secret` only while pressed; releasing, cancelling or leaving hides it again. */
export function Hold({ label, children, class: cls }: { label: string; children: ComponentChildren; class?: string }) {
  const [shown, setShown] = useState(false);
  const hide = () => setShown(false);
  return (
    <div
      class={`hold ${cls ?? ''} ${shown ? 'shown' : ''}`}
      role="button"
      aria-pressed={shown}
      tabIndex={0}
      onPointerDown={(e) => {
        (e.currentTarget as HTMLElement).setPointerCapture?.(e.pointerId);
        setShown(true);
      }}
      onPointerUp={hide}
      onPointerCancel={hide}
      onLostPointerCapture={hide}
      onKeyDown={(e) => e.key === ' ' && setShown(true)}
      onKeyUp={hide}
      onBlur={hide}
      onContextMenu={(e) => e.preventDefault()}
    >
      {shown ? children : <span class="hold-label">{label}</span>}
    </div>
  );
}

export function PhaseTitle({ phase, round }: { phase: string; round?: number }) {
  return <>{tOr(`phase_${phase}`, phase, { n: round ?? 1 })}</>;
}
