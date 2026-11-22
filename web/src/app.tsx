// Root: owns the link to the box and picks the screen from the latest view
// (docs/protocol.md "View"): profile, lobby, game, plus the admin's tabs.

import { useEffect, useMemo, useRef, useState } from 'preact/hooks';
import { AdminTab, DEFAULT_VAMPIRES } from './admin';
import { Board, narration, PhaseHeader, SeatList } from './board';
import { clientId, initialLinkState, Link, type LinkState } from './connection';
import { buzz, keepScreenOn, speak } from './device';
import { Diag } from './diag';
import { type Lang, lang, setLang, t, tOr } from './i18n';
import { Lobby } from './lobby';
import { ProfileScreen } from './profile';
import type { GameView, VampiresOptions, View } from './types';
import { type AppCtx, Ctx, type Send } from './ui';
import { VampiresScreen } from './vampires';

type Tab = 'main' | 'table' | 'admin';

interface Toast {
  id: number;
  text: string;
}

const LANGS: Lang[] = ['en', 'tr'];
const TOAST_MS = 3500;

/** Fills in what a box (or a future protocol) might leave out, so screens can trust the shape. */
function normalize(v: Partial<View>): View {
  return {
    ...v,
    me: v.me ?? -1,
    profile: v.profile ?? null,
    admin: v.admin ?? -1,
    isAdmin: !!v.isAdmin,
    full: !!v.full,
    players: v.players ?? [],
    games: v.games ?? [],
    endsIn: typeof v.endsIn === 'number' ? v.endsIn : -1,
  };
}

function GameScreen({ g }: { g: GameView }) {
  if (g.id === 'vampires') return <VampiresScreen g={g} />;
  return (
    <section class="card">
      <PhaseHeader g={g} />
      <SeatList seats={g.players} />
    </section>
  );
}

let toastIds = 0;

export function App() {
  const id = useMemo(clientId, []);
  const [, rerender] = useState<Lang>(lang());
  const [link, setLink] = useState<LinkState>(initialLinkState);
  const [snap, setSnap] = useState<{ view: View; deadline: number | null } | null>(null);
  const [toasts, setToasts] = useState<Toast[]>([]);
  const [hash, setHash] = useState(location.hash);
  const [tab, setTab] = useState<Tab>('main');
  const [editing, setEditing] = useState(false);
  const [narrate, setNarrate] = useState(false);
  const [opts, setOpts] = useState<VampiresOptions>(DEFAULT_VAMPIRES);
  const linkRef = useRef<Link | null>(null);
  const spokenPhase = useRef(-1);

  useEffect(() => {
    document.documentElement.lang = lang();
    keepScreenOn();
    const onHash = () => setHash(location.hash);
    window.addEventListener('hashchange', onHash);
    return () => window.removeEventListener('hashchange', onHash);
  }, []);

  useEffect(() => {
    const l = new Link(id, {
      state: setLink,
      view: (raw) => {
        const view = normalize(raw);
        // endsIn counts from now: turn it into a local deadline once, on receipt.
        setSnap({ view, deadline: view.endsIn >= 0 ? performance.now() + view.endsIn : null });
      },
      err: (code) => {
        const toast = { id: ++toastIds, text: tOr(`err_${code}`, t('err_unknown', { code })) };
        setToasts((list) => [...list.slice(-2), toast]);
        window.setTimeout(() => setToasts((list) => list.filter((x) => x !== toast)), TOAST_MS);
      },
      buzz,
    });
    linkRef.current = l;
    l.start();
    return () => l.stop();
  }, [id]);

  const view = snap?.view;
  const board = view?.board;
  const gameRunning = !!view?.game;

  // A new game (or its end) brings everyone back to their own screen.
  useEffect(() => setTab('main'), [gameRunning]);

  useEffect(() => {
    if (!narrate || !board || !view || board.phaseId === spokenPhase.current) return;
    spokenPhase.current = board.phaseId;
    const text = narration(board, view);
    if (text) speak(text, lang());
  }, [narrate, board?.phaseId, board?.phase]);

  const chooseLang = (next: Lang) => {
    setLang(next);
    document.documentElement.lang = next;
    rerender(next);
  };

  const send: Send = (msg) => {
    linkRef.current?.send(msg);
  };

  const ctx: AppCtx | null = snap && {
    view: snap.view,
    deadline: snap.deadline,
    send,
    narrate,
    setNarrate: (on) => {
      // Speaking inside the tap unlocks speech on iOS.
      if (on && board && view) {
        spokenPhase.current = board.phaseId;
        const text = narration(board, view);
        if (text) speak(text, lang());
      }
      setNarrate(on);
    },
  };

  let content = null;
  let tabs: Tab[] = [];
  if (hash === '#diag') {
    content = <Diag link={link} />;
  } else if (view) {
    if (view.me < 0 || editing) {
      content = <ProfileScreen onDone={view.me >= 0 ? () => setEditing(false) : undefined} />;
    } else {
      const seated = !!view.game?.seated;
      tabs = ['main'];
      if (view.isAdmin && seated && board) tabs.push('table');
      if (view.isAdmin) tabs.push('admin');
      const current = tabs.includes(tab) ? tab : 'main';
      content =
        current === 'admin' ? (
          <AdminTab opts={opts} setOpts={setOpts} />
        ) : current === 'table' && board ? (
          <Board board={board} />
        ) : seated && view.game ? (
          <GameScreen g={view.game} />
        ) : (
          <Lobby onEditProfile={() => setEditing(true)} />
        );
      if (tabs.length < 2) tabs = [];
      else
        content = (
          <>
            <nav class="tabs" role="tablist">
              {tabs.map((x) => (
                <button key={x} role="tab" aria-selected={x === current} onClick={() => setTab(x)}>
                  {t(x === 'main' ? (seated ? 'tab_card' : 'tab_lobby') : x === 'table' ? 'tab_table' : 'tab_admin')}
                </button>
              ))}
            </nav>
            {content}
          </>
        );
    }
  }

  const offline = hash !== '#diag' && (link.status !== 'open' || !view);

  return (
    <>
      <main>
        <header class="title-row">
          <h1>Party Games</h1>
          <div class="lang" role="group" aria-label="Language">
            {LANGS.map((l) => (
              <button key={l} aria-pressed={l === lang()} onClick={() => chooseLang(l)}>
                {l.toUpperCase()}
              </button>
            ))}
          </div>
        </header>
        {ctx ? <Ctx.Provider value={ctx}>{content}</Ctx.Provider> : content}
      </main>

      {offline && (
        <div class="overlay" role="status">
          <div class="spinner" />
          <strong>{t(link.status === 'open' ? 'status_connecting' : `status_${link.status}`)}</strong>
          <p class="muted">{t('connectingHint')}</p>
        </div>
      )}

      <div class="toasts" aria-live="assertive">
        {toasts.map((x) => (
          <div key={x.id} class="toast" onClick={() => setToasts((list) => list.filter((y) => y !== x))}>
            {x.text}
          </div>
        ))}
      </div>
    </>
  );
}
