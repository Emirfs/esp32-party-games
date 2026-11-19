import { useEffect, useMemo, useState } from 'preact/hooks';
import { clientId, Link, type LinkState } from './connection';

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

// esp_reset_reason_t
const RESET_REASONS: Record<number, string> = {
  1: 'güç verildi',
  2: 'reset pini',
  3: 'yazılım',
  4: 'çökme (panic)',
  5: 'kesme watchdog',
  6: 'görev watchdog',
  7: 'watchdog',
  9: 'düşük voltaj (brownout)',
  11: 'USB',
};

const STATUS_TEXT: Record<LinkState['status'], string> = {
  connecting: 'Bağlanıyor…',
  open: 'Bağlı',
  closed: 'Koptu',
};

function kb(bytes: number): string {
  return `${Math.round(bytes / 1024)} KB`;
}

function duration(ms: number): string {
  const s = Math.floor(ms / 1000);
  return `${Math.floor(s / 3600)}sa ${Math.floor((s % 3600) / 60)}dk ${s % 60}sn`;
}

function ms(v: number | null): string {
  return v === null ? '–' : `${v} ms`;
}

export function App() {
  const id = useMemo(clientId, []);
  const [link, setLink] = useState<LinkState | null>(null);
  const [box, setBox] = useState<BoxStatus | null>(null);
  const [vibe, setVibe] = useState<string | null>(null);

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
    const t = window.setInterval(poll, 5000);
    return () => window.clearInterval(t);
  }, []);

  const testVibration = () => {
    // iOS Safari has no Vibration API at all.
    if (!('vibrate' in navigator)) return setVibe('Bu tarayıcı titreşimi desteklemiyor');
    setVibe(navigator.vibrate([200, 100, 200]) ? 'Titreşim gönderildi' : 'Tarayıcı titreşimi reddetti');
  };

  return (
    <main>
      <header>
        <h1>Oyun Kutusu</h1>
        <p class="muted">Bağlantı testi · cihaz {id.slice(0, 6)}</p>
      </header>

      <section class="card">
        <div class={`pill ${link?.status ?? 'connecting'}`}>{STATUS_TEXT[link?.status ?? 'connecting']}</div>
        <dl>
          <dt>Gecikme</dt>
          <dd>{ms(link?.rttMs ?? null)}</dd>
          <dt>Yeniden bağlanma</dt>
          <dd>{link?.reconnects ?? 0}</dd>
          <dt>Son kopukluk</dt>
          <dd>{ms(link?.lastGapMs ?? null)}</dd>
          <dt>Ekran açılınca</dt>
          <dd>{ms(link?.resumeMs ?? null)}</dd>
        </dl>
      </section>

      <section class="card">
        <h2>Masadakiler ({link?.peers.length ?? 0})</h2>
        <ul class="peers">
          {link?.peers.map((p) => (
            <li key={p.id} class={p.id === id ? 'me' : undefined}>
              <span>{p.id.slice(0, 6)}</span>
              <span class="muted">
                {p.rtt >= 0 ? `${p.rtt} ms` : '–'} · {Math.round(p.age / 1000)} sn önce
              </span>
            </li>
          ))}
        </ul>
      </section>

      <section class="card">
        <h2>Kutu</h2>
        {box ? (
          <dl>
            <dt>Ağ</dt>
            <dd>{box.ssid}</dd>
            <dt>Wi-Fi / WebSocket</dt>
            <dd>
              {box.stations} / {box.wsClients}
            </dd>
            <dt>Açık kalma</dt>
            <dd>{duration(box.uptimeMs)}</dd>
            <dt>Son açılış nedeni</dt>
            <dd>{RESET_REASONS[box.resetReason] ?? `kod ${box.resetReason}`}</dd>
            <dt>RAM boş (en düşük)</dt>
            <dd>
              {kb(box.heapFree)} ({kb(box.heapMin)})
            </dd>
            <dt>PSRAM boş</dt>
            <dd>{box.psramTotal ? `${kb(box.psramFree)} / ${kb(box.psramTotal)}` : 'yok'}</dd>
            <dt>Sürüm</dt>
            <dd>{box.version}</dd>
          </dl>
        ) : (
          <p class="muted">Durum alınamadı</p>
        )}
      </section>

      <section class="card">
        <h2>Titreşim</h2>
        <button onClick={testVibration}>Titreşimi dene</button>
        {vibe && <p class="muted">{vibe}</p>}
      </section>

      <section class="card">
        <h2>Olaylar</h2>
        <ol class="log">
          {link?.log.map((line, i) => <li key={i}>{line}</li>)}
        </ol>
      </section>
    </main>
  );
}
