import type { ComponentChildren } from 'preact';
import { Board } from './board';
import { t } from './i18n';
import type { Player } from './types';
import { Avatar, useApp } from './ui';

export function PlayerRow({ p, children }: { p: Player; children?: ComponentChildren }) {
  const { view } = useApp();
  return (
    <li class={p.online ? 'player' : 'player offline'}>
      <Avatar pid={p.pid} />
      <span class="player-name">
        <span class={p.online ? 'dot on' : 'dot'} title={p.online ? undefined : t('offline')} />
        {p.name}
        {p.pid === view.me && <span class="muted"> ({t('you')})</span>}
        {p.pid === view.admin && (
          <span class="crown" title={t('admin')}>
            👑
          </span>
        )}
      </span>
      {view.game && p.inGame && <span class="tag">{t('inGame')}</span>}
      {!view.game && p.ready && (
        <span class="check" aria-label="✓">
          ✓
        </span>
      )}
      {children}
    </li>
  );
}

export function Lobby({ onEditProfile }: { onEditProfile: () => void }) {
  const { view, send } = useApp();
  const me = view.players.find((p) => p.pid === view.me);
  const prof = view.profile;
  const readyN = view.players.filter((p) => p.ready).length;

  return (
    <>
      <section class="card me-card">
        {me && <Avatar pid={me.pid} />}
        <div class="grow">
          <strong>{me?.name ?? prof?.name}</strong>
          {prof && (prof.played ?? 0) > 0 && <div class="muted small">{t('stats', { p: prof.played ?? 0, w: prof.won ?? 0 })}</div>}
        </div>
        <button class="secondary" onClick={onEditProfile}>
          {t('editProfile')}
        </button>
      </section>

      {view.admin < 0 && (
        <section class="card stack">
          <p class="muted">{t('noAdmin')}</p>
          <button class="primary" onClick={() => send({ t: 'claimAdmin' })}>
            {t('becomeAdmin')}
          </button>
        </section>
      )}

      {view.game ? (
        <>
          <p class="notice">{t('gameRunning')}</p>
          {view.board && <Board board={view.board} />}
        </>
      ) : (
        <section class="card stack">
          <h2>{t('readyQ')}</h2>
          <div class="segmented" role="group">
            <button aria-pressed={!!me?.ready} onClick={() => send({ t: 'ready', on: true })}>
              ✓ {t('ready')}
            </button>
            <button aria-pressed={!me?.ready} onClick={() => send({ t: 'ready', on: false })}>
              {t('notReady')}
            </button>
          </div>
          {!view.isAdmin && view.admin >= 0 && <p class="muted small">{t('waitAdmin')}</p>}
        </section>
      )}

      <section class="card">
        <h2>
          {t('players', { n: view.players.length })}
          {!view.game && <span class="muted"> · {t('ackCount', { n: readyN, total: view.players.length })}</span>}
        </h2>
        <ul class="players">
          {view.players.map((p) => (
            <PlayerRow key={p.pid} p={p} />
          ))}
        </ul>
      </section>
    </>
  );
}
