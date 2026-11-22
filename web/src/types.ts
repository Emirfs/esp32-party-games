// Shapes of the box's `view` message (docs/protocol.md). Keys the protocol
// marks optional, and game keys that only exist in some phases, are optional
// here too: the UI must survive any of them missing.

export interface Profile {
  name: string;
  avatar: string;
  color: number;
  played?: number;
  won?: number;
}

export interface Player {
  pid: number;
  name: string;
  avatar: string;
  color: number;
  online: boolean;
  ready: boolean;
  inGame: boolean;
}

export interface GameInfo {
  id: string;
  min: number;
  max: number;
}

export interface BoxInfo {
  ssid: string;
  wifiPass: string;
}

export interface Seat {
  pid: number;
  alive: boolean;
  role?: string;
}

export interface Dawn {
  died: number;
  suspicion?: { pid: number; n: number }[];
}

export interface VoteState {
  round: number;
  candidates: number[];
  /** -2 = not voted, -1 = nobody. Absent on the board. */
  mine?: number;
  cast: number;
  voters: number;
}

export interface Tally {
  round: number;
  votes: { voter: number; target: number }[];
  lynched: number;
  role?: string;
  runoff?: boolean;
}

export interface VampiresOptions {
  revealRole: boolean;
  ghostsSeeRoles: boolean;
  suspicion: boolean;
  discussionSec: number;
  voteSec: number;
}

/** Keys shared by the private game view and the public board. */
export interface TableView {
  id: string;
  phase: string;
  phaseId: number;
  round?: number;
  players?: Seat[];
  dawn?: Dawn;
  vote?: VoteState;
  tally?: Tally;
  winner?: string;
}

export interface GameView extends TableView {
  seated: boolean;
  options?: Partial<VampiresOptions>;
  role?: string;
  alive?: boolean;
  acked?: boolean;
  ackCount?: number;
  ackTotal?: number;
  task?: { kind: string; targets: number[]; pick: number };
  allies?: { pid: number; pick: number }[];
  seen?: { pid: number; vampire: boolean }[];
}

export interface BoardView extends TableView {
  acted?: number;
  actors?: number;
}

export interface View {
  me: number;
  profile: Profile | null;
  admin: number;
  isAdmin: boolean;
  full: boolean;
  players: Player[];
  games: GameInfo[];
  endsIn: number;
  box?: BoxInfo;
  game?: GameView;
  board?: BoardView;
}
