// Phone features the game leans on, each degrading silently: keeping the
// screen on, the `buzz` pattern and spoken narration.

import keepAwakeMp4 from './assets/keep-awake.mp4?inline';
import keepAwakeWebm from './assets/keep-awake.webm?inline';
import type { Lang } from './i18n';

/**
 * Wake Lock needs HTTPS, so a silent, muted, looping 16×16 video keeps the
 * screen on instead (H.264 for iPhones, VP8 for browsers without H.264; both
 * generated with ffmpeg from a black frame, under 1 KB). Browsers only start
 * playback inside a user gesture, so every tap (re)starts it; it is a no-op
 * while it already plays.
 */
export function keepScreenOn(): void {
  const video = document.createElement('video');
  video.className = 'keep-awake';
  video.muted = true;
  video.loop = true;
  video.setAttribute('muted', '');
  video.setAttribute('playsinline', '');
  video.setAttribute('aria-hidden', 'true');
  for (const [src, type] of [
    [keepAwakeMp4, 'video/mp4'],
    [keepAwakeWebm, 'video/webm'],
  ]) {
    const source = document.createElement('source');
    source.src = src;
    source.type = type;
    video.appendChild(source);
  }
  document.body.appendChild(video);
  const play = () => {
    if (video.paused) video.play().catch(() => {});
  };
  // pointerdown is no user activation on touch screens; touchend and click are.
  for (const type of ['click', 'touchend', 'keydown']) document.addEventListener(type, play, { capture: true, passive: true });
  document.addEventListener('visibilitychange', () => {
    if (document.visibilityState === 'visible') play();
  });
}

let flashTimers: number[] = [];

/** Shows the full-screen flash overlay for the pattern's on-durations (iPhone has no Vibration API). */
export function flash(pattern: number[]): void {
  let el = document.getElementById('flash');
  if (!el) {
    el = document.createElement('div');
    el.id = 'flash';
    document.body.appendChild(el);
  }
  const overlay = el;
  flashTimers.forEach((id) => window.clearTimeout(id));
  flashTimers = [];
  let at = 0;
  pattern.forEach((ms, i) => {
    const on = i % 2 === 0;
    flashTimers.push(window.setTimeout(() => overlay.classList.toggle('on', on), at));
    at += ms;
  });
  flashTimers.push(window.setTimeout(() => overlay.classList.remove('on'), at));
}

/** `buzz`: vibrate where the browser can, otherwise flash the screen. */
export function buzz(raw: number[]): void {
  const pattern = raw.filter((n) => Number.isFinite(n) && n >= 0).slice(0, 32).map((n) => Math.min(n, 5000));
  if (!pattern.length) return;
  try {
    if ('vibrate' in navigator && navigator.vibrate(pattern)) return;
  } catch {
    // fall through to the flash
  }
  flash(pattern);
}

export function canSpeak(): boolean {
  return 'speechSynthesis' in window && typeof SpeechSynthesisUtterance !== 'undefined';
}

export function speak(text: string, lang: Lang): void {
  if (!canSpeak()) return;
  try {
    const u = new SpeechSynthesisUtterance(text);
    u.lang = lang === 'tr' ? 'tr-TR' : 'en-US';
    speechSynthesis.cancel();
    speechSynthesis.speak(u);
  } catch {
    // narration is optional
  }
}
