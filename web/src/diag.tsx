// #diag: the phase 0 field test readouts (link timings, /api/status, vibration).

import { useEffect, useState } from 'preact/hooks';
import type { LinkState } from './connection';
import { flash } from './device';
import { isKey, type Key, lang, t } from './i18n';

interface BoxStatus {
  ssid: string;
  version: string;
  uptimeMs: number;
  resetReason: number;
  heapFree: number;
  heapMin: number;
  psramFree: number;
  psramTotal: number;
  stations: number;
  wsClients: number;
}

const TEST_PATTERN = [200, 100, 200];

function kb(bytes: number): string {
  return `${Math.round(bytes / 1024)} KB`;
}

function ms(v: number | null): string {
  return v === null ? '–' : `${v} ms`;
}

export function Diag({ link }: { link: LinkState }) {
  const [box, setBox] = useState<BoxStatus | null>(null);
  const [vibe, setVibe] = useState<Key | null>(null);

  useEffect(() => {
    const poll = () =>
      fetch('/api/status')
        .then((r) => r.json())
        .then(setBox)
        .catch(() => setBox(null));
    poll();
    const timer = window.setInterval(poll, 5000);
    return () => window.clearInterval(timer);
  }, []);

  const testVibration = () => {
    // iOS Safari has no Vibration API at all.
    const key: Key = !('vibrate' in navigator) ? 'vibeUnsupported' : navigator.vibrate(TEST_PATTERN) ? 'vibeSent' : 'vibeRefused';
    if (key !== 'vibeSent') flash(TEST_PATTERN);
    setVibe(key);
  };

  const resetKey = box ? `reset_${box.resetReason}` : '';
  const uptimeS = box ? Math.floor(box.uptimeMs / 1000) : 0;

  return (
    <>
      <a class="link" href="#">
        ← {t('back')}
      </a>

      <section class="card">
        <h2>{t('diagnostics')}</h2>
        <div class={`pill ${link.status}`}>{t(`status_${link.status}`)}</div>
        <dl>
          <dt>{t('latency')}</dt>
          <dd>{ms(link.rttMs)}</dd>
          <dt>{t('reconnects')}</dt>
          <dd>{link.reconnects}</dd>
          <dt>{t('lastGap')}</dt>
          <dd>{ms(link.lastGapMs)}</dd>
          <dt>{t('resume')}</dt>
          <dd>{ms(link.resumeMs)}</dd>
        </dl>
      </section>

      <section class="card">
        <h2>{t('box')}</h2>
        {box ? (
          <dl>
            <dt>{t('network')}</dt>
            <dd>{box.ssid}</dd>
            <dt>{t('wifiWs')}</dt>
            <dd>
              {box.stations} / {box.wsClients}
            </dd>
            <dt>{t('uptime')}</dt>
            <dd>{t('duration', { h: Math.floor(uptimeS / 3600), m: Math.floor((uptimeS % 3600) / 60), s: uptimeS % 60 })}</dd>
            <dt>{t('resetReason')}</dt>
            <dd>{isKey(resetKey) ? t(resetKey) : t('resetCode', { n: box.resetReason })}</dd>
            <dt>{t('ramFree')}</dt>
            <dd>
              {kb(box.heapFree)} ({kb(box.heapMin)})
            </dd>
            <dt>{t('psramFree')}</dt>
            <dd>{box.psramTotal ? `${kb(box.psramFree)} / ${kb(box.psramTotal)}` : t('none')}</dd>
            <dt>{t('version')}</dt>
            <dd>{box.version}</dd>
          </dl>
        ) : (
          <p class="muted">{t('statusUnavailable')}</p>
        )}
      </section>

      <section class="card stack">
        <h2>{t('vibration')}</h2>
        <button class="primary" onClick={testVibration}>
          {t('tryVibration')}
        </button>
        {vibe && <p class="muted">{t(vibe)}</p>}
      </section>

      <section class="card">
        <h2>{t('events')}</h2>
        <ol class="log">
          {link.log.map((e) => (
            <li key={`${e.at}-${e.event}`}>
              {new Date(e.at).toLocaleTimeString(lang())} {t(`log_${e.event}`, { ms: e.ms ?? '' })}
            </li>
          ))}
        </ol>
      </section>
    </>
  );
}
