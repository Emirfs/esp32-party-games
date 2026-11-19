// UI strings. English is the default and the fallback; Turkish is chosen when
// the phone's language is Turkish or the player picks it.

export type Lang = 'en' | 'tr';

const LANG_KEY = 'pg.lang';

const en = {
  subtitle: 'Connection test · device {id}',
  status_connecting: 'Connecting…',
  status_open: 'Connected',
  status_closed: 'Disconnected',
  latency: 'Latency',
  reconnects: 'Reconnects',
  lastGap: 'Last outage',
  resume: 'After screen on',
  atTable: 'At the table ({n})',
  secondsAgo: '{n} s ago',
  box: 'Box',
  network: 'Network',
  wifiWs: 'Wi-Fi / WebSocket',
  uptime: 'Uptime',
  duration: '{h}h {m}m {s}s',
  resetReason: 'Last boot reason',
  resetCode: 'code {n}',
  reset_1: 'power on',
  reset_2: 'reset pin',
  reset_3: 'software',
  reset_4: 'crash (panic)',
  reset_5: 'interrupt watchdog',
  reset_6: 'task watchdog',
  reset_7: 'watchdog',
  reset_9: 'low voltage (brownout)',
  reset_11: 'USB',
  ramFree: 'RAM free (lowest)',
  psramFree: 'PSRAM free',
  none: 'none',
  version: 'Version',
  statusUnavailable: 'Status unavailable',
  vibration: 'Vibration',
  tryVibration: 'Try vibration',
  vibeUnsupported: 'This browser does not support vibration',
  vibeSent: 'Vibration sent',
  vibeRefused: 'The browser refused to vibrate',
  events: 'Events',
  log_connected: 'connected',
  log_reconnected: 'reconnected ({ms} ms outage)',
  log_lost: 'connection lost',
  log_pongTimeout: 'no pong, reconnecting',
  log_resumed: 'screen on, live after {ms} ms',
};

export type Key = keyof typeof en;

const tr: Record<Key, string> = {
  subtitle: 'Bağlantı testi · cihaz {id}',
  status_connecting: 'Bağlanıyor…',
  status_open: 'Bağlı',
  status_closed: 'Koptu',
  latency: 'Gecikme',
  reconnects: 'Yeniden bağlanma',
  lastGap: 'Son kopukluk',
  resume: 'Ekran açılınca',
  atTable: 'Masadakiler ({n})',
  secondsAgo: '{n} sn önce',
  box: 'Kutu',
  network: 'Ağ',
  wifiWs: 'Wi-Fi / WebSocket',
  uptime: 'Açık kalma',
  duration: '{h}sa {m}dk {s}sn',
  resetReason: 'Son açılış nedeni',
  resetCode: 'kod {n}',
  reset_1: 'güç verildi',
  reset_2: 'reset pini',
  reset_3: 'yazılım',
  reset_4: 'çökme (panic)',
  reset_5: 'kesme watchdog',
  reset_6: 'görev watchdog',
  reset_7: 'watchdog',
  reset_9: 'düşük voltaj (brownout)',
  reset_11: 'USB',
  ramFree: 'RAM boş (en düşük)',
  psramFree: 'PSRAM boş',
  none: 'yok',
  version: 'Sürüm',
  statusUnavailable: 'Durum alınamadı',
  vibration: 'Titreşim',
  tryVibration: 'Titreşimi dene',
  vibeUnsupported: 'Bu tarayıcı titreşimi desteklemiyor',
  vibeSent: 'Titreşim gönderildi',
  vibeRefused: 'Tarayıcı titreşimi reddetti',
  events: 'Olaylar',
  log_connected: 'bağlandı',
  log_reconnected: 'yeniden bağlandı ({ms} ms kopukluk)',
  log_lost: 'bağlantı koptu',
  log_pongTimeout: 'pong gelmedi, bağlantı yenileniyor',
  log_resumed: 'ekran açıldı, {ms} ms sonra canlı',
};

const STRINGS: Record<Lang, Record<Key, string>> = { en, tr };

export function isKey(key: string): key is Key {
  return key in en;
}

export function initialLang(): Lang {
  const saved = localStorage.getItem(LANG_KEY);
  if (saved === 'en' || saved === 'tr') return saved;
  return navigator.language.toLowerCase().startsWith('tr') ? 'tr' : 'en';
}

export function saveLang(lang: Lang): void {
  localStorage.setItem(LANG_KEY, lang);
}

export function translate(lang: Lang, key: Key, params: Record<string, string | number> = {}): string {
  return STRINGS[lang][key].replace(/\{(\w+)\}/g, (_, name: string) => String(params[name] ?? ''));
}
