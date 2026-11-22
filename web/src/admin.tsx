// Admin tab: start a game with its options, run the current one, manage
// players and the box. Never shows hidden information.

import { useState } from 'preact/hooks';
import { t, tOr } from './i18n';
import { PlayerRow } from './lobby';
import { Qr, wifiQrText } from './qr';
import type { GameInfo, VampiresOptions } from './types';
import { nameOf, PhaseTitle, useApp } from './ui';

export const DEFAULT_VAMPIRES: VampiresOptions = {
  revealRole: true,
  ghostsSeeRoles: true,
  suspicion: true,
  discussionSec: 180,
  voteSec: 60,
};

const FLAGS = ['revealRole', 'ghostsSeeRoles', 'suspicion'] as const;
const NUMBERS = [
  ['discussionSec', 30, 600, 30],
  ['voteSec', 20, 180, 10],
] as const;
const BOX_URL = 'http://192.168.4.1';

function VampiresForm({ opts, onChange }: { opts: VampiresOptions; onChange: (o: VampiresOptions) => void }) {
  return (
    <fieldset class="stack">
      <legend>{t('options')}</legend>
      {FLAGS.map((k) => (
        <label class="switch" key={k}>
          <input type="checkbox" checked={opts[k]} onChange={(e) => onChange({ ...opts, [k]: e.currentTarget.checked })} />
          <span>{t(`opt_${k}`)}</span>
        </label>
      ))}
      {NUMBERS.map(([k, min, max, step]) => (
        <label class="field" key={k}>
          <span>{t(`opt_${k}`)}</span>
          <input
            type="number"
            inputMode="numeric"
            min={min}
            max={max}
            step={step}
            value={opts[k]}
            onChange={(e) => {
              const n = Math.round(Number(e.currentTarget.value));
              onChange({ ...opts, [k]: Number.isFinite(n) ? Math.min(max, Math.max(min, n)) : DEFAULT_VAMPIRES[k] });
            }}
          />
        </label>
      ))}
    </fieldset>
  );
}

function StartGame({ opts, setOpts }: { opts: VampiresOptions; setOpts: (o: VampiresOptions) => void }) {
  const { view, send } = useApp();
  const games = view.games ?? [];
  const [gameId, setGameId] = useState(games[0]?.id ?? '');
  const game: GameInfo | undefined = games.find((g) => g.id === gameId) ?? games[0];
  if (!game) return null;
  const ready = view.players.filter((p) => p.ready).length;
  const fits = ready >= game.min && ready <= game.max;
  return (
    <section class="card stack">
      <h2>{t('game')}</h2>
      {games.length > 1 && (
        <div class="segmented" role="group">
          {games.map((g) => (
            <button key={g.id} aria-pressed={g.id === game.id} onClick={() => setGameId(g.id)}>
              {tOr(`game_${g.id}`, g.id)}
            </button>
          ))}
        </div>
      )}
      {games.length === 1 && <strong>{tOr(`game_${game.id}`, game.id)}</strong>}
      {game.id === 'vampires' && <VampiresForm opts={opts} onChange={setOpts} />}
      <button
        class="primary"
        disabled={!fits}
        onClick={() => send({ t: 'start', game: game.id, options: game.id === 'vampires' ? opts : {} })}
      >
        {t('start', { n: ready })}
      </button>
      {!fits && <p class="muted small center">{t('needPlayers', { min: game.min, max: game.max })}</p>}
    </section>
  );
}

function RunningGame() {
  const { view, send } = useApp();
  const g = view.game ?? view.board;
  if (!g) return null;
  const ended = g.phase === 'end';
  return (
    <section class="card stack">
      <h2>
        {tOr(`game_${g.id}`, g.id)} · <PhaseTitle phase={g.phase} round={g.round} />
      </h2>
      {!ended && (
        <button class="secondary" disabled={g.phase === 'night'} onClick={() => send({ t: 'skip' })}>
          {g.phase === 'discussion' ? t('endDiscussion') : t('skip')}
        </button>
      )}
      <button
        class={ended ? 'primary' : 'danger'}
        onClick={() => (ended || confirm(t('confirmEnd'))) && send({ t: 'end' })}
      >
        {ended ? t('backToLobby') : t('endGame')}
      </button>
    </section>
  );
}

function WifiSettings() {
  const { send } = useApp();
  const [pass, setPass] = useState('');
  const valid = /^[\x20-\x7e]{8,63}$/.test(pass);
  return (
    <section class="card stack">
      <h2>{t('wifiPass')}</h2>
      <form
        class="row"
        onSubmit={(e) => {
          e.preventDefault();
          if (!valid) return;
          send({ t: 'settings', wifiPass: pass });
          setPass('');
        }}
      >
        <input class="grow" value={pass} maxLength={63} autoComplete="off" autoCapitalize="off" onInput={(e) => setPass(e.currentTarget.value)} />
        <button class="secondary" type="submit" disabled={!valid}>
          {t('save')}
        </button>
      </form>
      <p class="muted small">{t('wifiHint')}</p>
      <button class="danger" onClick={() => confirm(t('confirmRestart')) && send({ t: 'restart' })}>
        {t('restart')}
      </button>
    </section>
  );
}

export function AdminTab({ opts, setOpts }: { opts: VampiresOptions; setOpts: (o: VampiresOptions) => void }) {
  const { view, send } = useApp();
  const box = view.box;
  return (
    <>
      {view.game ? <RunningGame /> : <StartGame opts={opts} setOpts={setOpts} />}

      <section class="card">
        <h2>{t('players', { n: view.players.length })}</h2>
        <ul class="players">
          {view.players.map((p) => (
            <PlayerRow key={p.pid} p={p}>
              {p.pid !== view.me && (
                <span class="row-actions">
                  <button
                    class="small secondary"
                    onClick={() => confirm(t('confirmTransfer', { name: nameOf(view, p.pid) })) && send({ t: 'transfer', pid: p.pid })}
                  >
                    {t('makeAdmin')}
                  </button>
                  {!view.game && (
                    <button
                      class="small danger"
                      onClick={() => confirm(t('confirmKick', { name: nameOf(view, p.pid) })) && send({ t: 'kick', pid: p.pid })}
                    >
                      {t('kick')}
                    </button>
                  )}
                </span>
              )}
            </PlayerRow>
          ))}
        </ul>
      </section>

      {box && (
        <section class="card">
          <h2>{t('joinBox')}</h2>
          <div class="qrs">
            <Qr text={wifiQrText(box.ssid, box.wifiPass)} label={`${t('qrWifi')}: ${box.ssid}`} />
            <Qr text={BOX_URL} label={`${t('qrUrl')}: 192.168.4.1`} />
          </div>
          <p class="muted small center">
            {box.ssid} · {box.wifiPass}
          </p>
        </section>
      )}

      {box && <WifiSettings />}

      <a class="link center" href="#diag">
        {t('diagnostics')}
      </a>
    </>
  );
}
