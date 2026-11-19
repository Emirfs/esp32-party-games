import { useEffect, useMemo, useState } from 'preact/hooks';
import { clientId, Link, type LinkState } from './connection';
import { initialLang, isKey, type Key, type Lang, saveLang, translate } from './i18n';

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

const LANGS: Lang[] = ['en', 'tr'];

function kb(bytes: number): string {
  return `${Math.round(bytes / 1024)} KB`;
}

function ms(v: number | null): string {
  return v === null ? '–' : `${v} ms`;
}

export function App() {
  const id = useMemo(clientId, []);
  const [lang, setLang] = useState<Lang>(initialLang);
  const [link, setLink] = useState<LinkState | null>(null);
  const [box, setBox] = useState<BoxStatus | null>(null);
  const [vibe, setVibe] = useState<Key | null>(null);
  const t = (key: Key, params?: Record<string, string | number>) => translate(lang, key, params);

  useEffect(() => {
    document.documentElement.lang = lang;
  }, [lang]);

  useEffect(() => {
    const l = new Link(id, setLink);
    l.start();
    return () => l.stop();
  }, [id]);

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

  const chooseLang = (next: Lang) => {
    saveLang(next);
    setLang(next);
  };

  const testVibration = () => {
    // iOS Safari has no Vibration API at all.
    if (!('vibrate' in navigator)) return setVibe('vibeUnsupported');
    setVibe(navigator.vibrate([200, 100, 200]) ? 'vibeSent' : 'vibeRefused');
  };

  const status = link?.status ?? 'connecting';
  const resetKey = box ? `reset_${box.resetReason}` : '';
  const uptimeS = box ? Math.floor(box.uptimeMs / 1000) : 0;

  return (
    <main>
      <header>
        <div class="title-row">
          <h1>Party Games</h1>
          <div class="lang" role="group" aria-label="Language">
            {LANGS.map((l) => (
              <button key={l} aria-pressed={l === lang} onClick={() => chooseLang(l)}>
                {l.toUpperCase()}
              </button>
            ))}
          </div>
        </div>
        <p class="muted">{t('subtitle', { id: id.slice(0, 6) })}</p>
      </header>

      <section class="card">
        <div class={`pill ${status}`}>{t(`status_${status}`)}</div>
        <dl>
          <dt>{t('latency')}</dt>
          <dd>{ms(link?.rttMs ?? null)}</dd>
          <dt>{t('reconnects')}</dt>
          <dd>{link?.reconnects ?? 0}</dd>
          <dt>{t('lastGap')}</dt>
          <dd>{ms(link?.lastGapMs ?? null)}</dd>
          <dt>{t('resume')}</dt>
          <dd>{ms(link?.resumeMs ?? null)}</dd>
        </dl>
      </section>

      <section class="card">
        <h2>{t('atTable', { n: link?.peers.length ?? 0 })}</h2>
        <ul class="peers">
          {link?.peers.map((p) => (
            <li key={p.id} class={p.id === id ? 'me' : undefined}>
              <span>{p.id.slice(0, 6)}</span>
              <span class="muted">
                {p.rtt >= 0 ? `${p.rtt} ms` : '–'} · {t('secondsAgo', { n: Math.round(p.age / 1000) })}
              </span>
            </li>
          ))}
        </ul>
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

      <section class="card">
        <h2>{t('vibration')}</h2>
        <button class="primary" onClick={testVibration}>
          {t('tryVibration')}
        </button>
        {vibe && <p class="muted">{t(vibe)}</p>}
      </section>

      <section class="card">
        <h2>{t('events')}</h2>
        <ol class="log">
          {link?.log.map((e) => (
            <li key={`${e.at}-${e.event}`}>
              {new Date(e.at).toLocaleTimeString(lang)} {t(`log_${e.event}`, { ms: e.ms ?? '' })}
            </li>
          ))}
        </ol>
      </section>
    </main>
  );
}
