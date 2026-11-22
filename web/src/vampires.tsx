// Vampires & Villagers, seated player screens. Every role gets the same
// layout and the same timing (everyone holds a card, everyone has a night
// task), so nobody's screen or behaviour gives their role away.

import type { ComponentChildren } from 'preact';
import { DawnSummary, PhaseHeader, SeatList, TallyView, Winner } from './board';
import { t, tOr } from './i18n';
import type { GameView } from './types';
import { Avatar, Countdown, Hold, nameOf, roleName, useApp, Who } from './ui';

function RoleFace({ g }: { g: GameView }) {
  const { view } = useApp();
  const role = g.role ?? '';
  const allies = role === 'vampire' ? (g.players ?? []).filter((s) => s.role === 'vampire' && s.pid !== view.me) : [];
  return (
    <div class={`role-face ${role}`}>
      <strong class="role-name">{roleName(role)}</strong>
      <p>{tOr(`roleDesc_${role}`, '')}</p>
      {allies.length > 0 && (
        <p>
          {t('fellowVampires')}: {allies.map((s) => nameOf(view, s.pid)).join(', ')}
        </p>
      )}
    </div>
  );
}

/** A grid of player tiles; `picked` is highlighted, `marks` adds small avatars (vampire allies' picks). */
function TargetGrid({
  targets,
  picked,
  onPick,
  nobody,
  marks,
}: {
  targets: number[];
  picked: number;
  onPick?: (pid: number) => void;
  nobody?: boolean;
  marks?: Map<number, number[]>;
}) {
  const { view } = useApp();
  const tile = (pid: number, content: ComponentChildren) => (
    <button key={pid} class="tile" aria-pressed={pid === picked} disabled={!onPick} onClick={() => onPick?.(pid)}>
      {content}
      {marks?.get(pid)?.map((a) => (
        <span class="mark" key={a}>
          <Avatar pid={a} small />
        </span>
      ))}
    </button>
  );
  return (
    <div class="tiles">
      {targets.map((pid) =>
        tile(
          pid,
          <>
            <Avatar pid={pid} />
            <span class="tile-name">{nameOf(view, pid)}</span>
          </>,
        ),
      )}
      {nobody && tile(-1, <span class="tile-name">{t('nobody')}</span>)}
    </div>
  );
}

function Seen({ g }: { g: GameView }) {
  if (!g.seen?.length) return null;
  return (
    <section class="card">
      <h2>{t('seen')}</h2>
      <ul class="seats">
        {g.seen.map((s) => (
          <li key={s.pid}>
            <Who pid={s.pid} small />
            <span class={s.vampire ? 'role-tag vampire' : 'role-tag'}>{s.vampire ? t('isVampire') : t('notVampire')}</span>
          </li>
        ))}
      </ul>
    </section>
  );
}

function RolePhase({ g }: { g: GameView }) {
  const { send } = useApp();
  return (
    <>
      <Hold class="role-card" label={t('holdToSee')}>
        <RoleFace g={g} />
      </Hold>
      <button class="primary" disabled={!!g.acked} onClick={() => send({ t: 'ack', phaseId: g.phaseId })}>
        {g.acked ? '✓ ' : ''}
        {t('gotIt')}
      </button>
      <p class="muted center">{t('ackCount', { n: g.ackCount ?? 0, total: g.ackTotal ?? 0 })}</p>
    </>
  );
}

function Night({ g }: { g: GameView }) {
  const { send } = useApp();
  const task = g.task;
  if (g.alive === false || !task) {
    return (
      <section class="card">
        <p class="headline">{t('nightWait')}</p>
      </section>
    );
  }
  const marks = new Map<number, number[]>();
  for (const a of g.allies ?? []) if (a.pick >= 0) marks.set(a.pick, [...(marks.get(a.pick) ?? []), a.pid]);
  return (
    <section class="card stack">
      <h2>{tOr(`task_${task.kind}`, task.kind)}</h2>
      <TargetGrid
        targets={task.targets ?? []}
        picked={task.pick}
        marks={marks}
        onPick={(pid) => send({ t: 'act', phaseId: g.phaseId, target: pid })}
      />
      <p class="muted small">{t('taskHint')}</p>
    </section>
  );
}

function Vote({ g }: { g: GameView }) {
  const { send } = useApp();
  const vote = g.vote;
  if (!vote) return null;
  const alive = g.alive !== false;
  return (
    <section class="card stack">
      <h2>{vote.round > 1 ? t('runoffTitle') : t('voteTitle')}</h2>
      {alive ? (
        <>
          <TargetGrid
            targets={vote.candidates ?? []}
            picked={vote.mine ?? -2}
            nobody
            onPick={(pid) => send({ t: 'vote', phaseId: g.phaseId, target: pid })}
          />
          <p class="muted small">{t('voteHint')}</p>
        </>
      ) : (
        <p class="muted">{t('ghostNoVote')}</p>
      )}
      <p class="center">{t('votesCast', { n: vote.cast, total: vote.voters })}</p>
    </section>
  );
}

function End({ g }: { g: GameView }) {
  const { view, send } = useApp();
  return (
    <>
      <Winner winner={g.winner} />
      {view.isAdmin ? (
        <button class="primary" onClick={() => send({ t: 'end' })}>
          {t('backToLobby')}
        </button>
      ) : (
        <p class="muted center">{t('waitLobby')}</p>
      )}
    </>
  );
}

export function VampiresScreen({ g }: { g: GameView }) {
  const { view, send } = useApp();
  let body: ComponentChildren = null;
  switch (g.phase) {
    case 'role':
      body = <RolePhase g={g} />;
      break;
    case 'night':
      body = <Night g={g} />;
      break;
    case 'dawn':
      body = <DawnSummary dawn={g.dawn} />;
      break;
    case 'discussion':
      body = (
        <>
          <div class="center">
            <Countdown big />
          </div>
          <DawnSummary dawn={g.dawn} />
          {view.isAdmin && (
            <button class="primary" onClick={() => send({ t: 'skip' })}>
              {t('endDiscussion')}
            </button>
          )}
        </>
      );
      break;
    case 'vote':
      body = <Vote g={g} />;
      break;
    case 'verdict':
      body = <TallyView tally={g.tally} />;
      break;
    case 'end':
      body = <End g={g} />;
      break;
  }
  return (
    <div class="stack">
      <section class="card">
        <PhaseHeader g={g} />
      </section>
      {g.alive === false && g.phase !== 'end' && <p class="notice">{t('ghost')}</p>}
      {body}
      {g.role && g.phase !== 'role' && g.phase !== 'end' && (
        // Seer results stay behind the hold too: an open list would tell the
        // table who the seer is.
        <Hold class="role-mini" label={t('holdRole')}>
          <RoleFace g={g} />
          <Seen g={g} />
        </Hold>
      )}
      {g.phase !== 'role' && (
        <section class="card">
          <SeatList seats={g.players} hideLiving={g.alive !== false && g.phase !== 'end'} />
        </section>
      )}
    </div>
  );
}
