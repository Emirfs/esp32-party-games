// Public table view (`board`) plus the result pieces the seated screens share.

import { canSpeak } from './device';
import { t, tOr } from './i18n';
import type { BoardView, Dawn, Seat, Tally, TableView, View } from './types';
import { Countdown, nameOf, PhaseTitle, roleName, useApp, Who } from './ui';

export function DawnSummary({ dawn }: { dawn?: Dawn }) {
  const { view } = useApp();
  if (!dawn) return null;
  const sus = dawn.suspicion ?? [];
  const max = Math.max(1, ...sus.map((s) => s.n));
  return (
    <>
      <p class="headline">{dawn.died >= 0 ? t('died', { name: nameOf(view, dawn.died) }) : t('nobodyDied')}</p>
      {sus.length > 0 && (
        <section class="card">
          <h2>{t('suspicion')}</h2>
          <ul class="bars">
            {[...sus]
              .sort((a, b) => b.n - a.n)
              .map((s) => (
                <li key={s.pid}>
                  <Who pid={s.pid} small />
                  <span class="bar" style={{ width: `${(s.n / max) * 100}%` }} />
                  <span class="num">{s.n}</span>
                </li>
              ))}
          </ul>
        </section>
      )}
    </>
  );
}

export function TallyView({ tally }: { tally?: Tally }) {
  const { view } = useApp();
  if (!tally) return null;
  const votes = tally.votes ?? [];
  return (
    <>
      {tally.runoff ? (
        <p class="headline">{t('runoff')}</p>
      ) : tally.lynched >= 0 ? (
        <>
          <p class="headline">{t('lynched', { name: nameOf(view, tally.lynched) })}</p>
          {tally.role && <p class="headline small">{t('wasRole', { name: nameOf(view, tally.lynched), role: roleName(tally.role) })}</p>}
        </>
      ) : (
        <p class="headline">{t('nobodyLynched')}</p>
      )}
      <section class="card">
        <h2>{t('votes')}</h2>
        {votes.length ? (
          <ul class="votes">
            {votes.map((v) => (
              <li key={v.voter}>
                <Who pid={v.voter} small />
                <span class="arrow">→</span>
                {v.target >= 0 ? <Who pid={v.target} small /> : <span class="muted">{t('nobody')}</span>}
              </li>
            ))}
          </ul>
        ) : (
          <p class="muted">{t('noVotes')}</p>
        )}
      </section>
    </>
  );
}

export function Winner({ winner }: { winner?: string }) {
  if (!winner) return null;
  return <p class={`headline winner ${winner}`}>{tOr(`win_${winner}`, winner)}</p>;
}

/**
 * Everyone in the game, dead ones struck through, with the roles this phone
 * may know. `hideLiving` keeps living players' roles (mine, fellow vampires)
 * off a living player's list, where anyone glancing at the phone would see them.
 */
export function SeatList({ seats, hideLiving }: { seats?: Seat[]; hideLiving?: boolean }) {
  const { view } = useApp();
  if (!seats?.length) return null;
  return (
    <ul class="seats">
      {seats.map((s) => {
        const p = view.players.find((x) => x.pid === s.pid);
        return (
          <li key={s.pid} class={s.alive ? undefined : 'dead'}>
            <Who pid={s.pid} small />
            {p && !p.online && <span class="dot" title={t('offline')} />}
            {s.role && !(hideLiving && s.alive) && <span class={`role-tag ${s.role}`}>{roleName(s.role)}</span>}
            {!s.alive && <span class="muted small">{t('dead')}</span>}
          </li>
        );
      })}
    </ul>
  );
}

export function PhaseHeader({ g }: { g: TableView }) {
  return (
    <div class="phase-bar">
      <h2>
        <PhaseTitle phase={g.phase} round={g.round} />
      </h2>
      <Countdown />
    </div>
  );
}

export function Board({ board }: { board: BoardView }) {
  const { narrate, setNarrate } = useApp();
  const actors = board.actors ?? 0;
  return (
    <div class="stack">
      <section class="card stack">
        <PhaseHeader g={board} />
        {(board.phase === 'role' || board.phase === 'night') && actors > 0 && (
          <p class="muted">{t(board.phase === 'role' ? 'ackCount' : 'chose', { n: board.acted ?? 0, total: actors })}</p>
        )}
        {board.phase === 'vote' && board.vote && (
          <p class="muted">
            {board.vote.round > 1 && `${t('runoffTitle')} · `}
            {t('votesCast', { n: board.vote.cast, total: board.vote.voters })}
          </p>
        )}
        {canSpeak() && (
          <label class="switch">
            <input type="checkbox" checked={narrate} onChange={(e) => setNarrate(e.currentTarget.checked)} />
            <span>{t('narration')}</span>
          </label>
        )}
      </section>
      {(board.phase === 'dawn' || board.phase === 'discussion') && <DawnSummary dawn={board.dawn} />}
      {board.phase === 'verdict' && <TallyView tally={board.tally} />}
      {board.phase === 'end' && <Winner winner={board.winner} />}
      <section class="card">
        <SeatList seats={board.players} />
      </section>
    </div>
  );
}

/** What the narrator says when the board enters a new phase; null = stay quiet. */
export function narration(board: BoardView, view: View): string | null {
  switch (board.phase) {
    case 'role':
      return t('say_role');
    case 'night':
      return t('say_night', { n: board.round ?? 1 });
    case 'dawn':
      return board.dawn ? (board.dawn.died >= 0 ? t('died', { name: nameOf(view, board.dawn.died) }) : t('nobodyDied')) : null;
    case 'discussion':
      return t('say_discussion');
    case 'vote':
      return t('say_vote');
    case 'verdict': {
      const tally = board.tally;
      if (!tally) return null;
      if (tally.runoff) return t('runoff');
      if (tally.lynched < 0) return t('nobodyLynched');
      const name = nameOf(view, tally.lynched);
      return `${t('lynched', { name })}${tally.role ? ` ${t('wasRole', { name, role: roleName(tally.role) })}` : ''}`;
    }
    case 'end':
      return board.winner ? tOr(`win_${board.winner}`, '') || null : null;
    default:
      return null;
  }
}
