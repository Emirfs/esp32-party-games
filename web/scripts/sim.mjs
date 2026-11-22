// Box simulator for UI work without a board: serves web/dist and bridges
// WebSockets at /ws to the host build of the firmware's game logic
// (`cd firmware && pio run -e host`), which speaks a line protocol on
// stdin/stdout (firmware/src/host/main.cpp).
//
//   npm run sim                      # http://localhost:7420 (built UI), ws://localhost:7420/ws
//   PG_BOX=http://localhost:7420 npm run dev   # live-reloading UI against the simulator
//
// Options: --port 7420, --fresh (forget profiles/admin/game), --host <binary>.
// Restarting the simulator keeps its state, like a power cut on the box.
import { spawn } from 'node:child_process';
import { createHash } from 'node:crypto';
import { existsSync, readFileSync, rmSync } from 'node:fs';
import { createServer } from 'node:http';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const here = dirname(fileURLToPath(import.meta.url));
const args = process.argv.slice(2);
const opt = (name, fallback) => {
  const i = args.indexOf(name);
  return i >= 0 && i + 1 < args.length ? args[i + 1] : fallback;
};
const port = Number(opt('--port', process.env.PORT ?? 7420));
const firmware = resolve(here, '../../firmware');
const exe = process.platform === 'win32' ? 'program.exe' : 'program';
const hostBin = resolve(opt('--host', join(firmware, '.pio/build/host', exe)));
const storeDir = join(firmware, '.pio/host-store');
const dist = resolve(here, '../dist');

if (!existsSync(hostBin)) {
  console.error(`sim: ${hostBin} not found; build it with: cd firmware && pio run -e host`);
  process.exit(1);
}
if (args.includes('--fresh')) rmSync(storeDir, { recursive: true, force: true });

// ------------------------------------------------------------ host process

const sockets = new Map(); // conn id -> socket
let host = null;
let startedAt = Date.now();

function startHost() {
  startedAt = Date.now();
  host = spawn(hostBin, ['--store', storeDir], { stdio: ['pipe', 'pipe', 'inherit'] });
  let pending = '';
  host.stdout.setEncoding('utf8');
  host.stdout.on('data', (chunk) => {
    pending += chunk;
    let nl;
    while ((nl = pending.indexOf('\n')) >= 0) {
      const line = pending.slice(0, nl);
      pending = pending.slice(nl + 1);
      onHostLine(line);
    }
  });
  host.on('exit', (code) => {
    console.log(`sim: host exited (${code}); restarting like a rebooting box`);
    host = null;
    for (const s of sockets.values()) s.destroy();
    sockets.clear();
    setTimeout(startHost, 1000);
  });
  for (const id of sockets.keys()) toHost(`O ${id}`);
}

function toHost(line) {
  host?.stdin.write(`${line}\n`);
}

function onHostLine(line) {
  const space = line.indexOf(' ', 2);
  if (line[0] === 'S') {
    const id = Number(line.slice(2, space));
    const socket = sockets.get(id);
    if (socket) sendText(socket, line.slice(space + 1));
  } else if (line[0] === 'X') {
    const id = Number(line.slice(2));
    sockets.get(id)?.end(Buffer.from([0x88, 0x00]));
  }
}

// ------------------------------------------------------------ WebSocket (RFC 6455, text frames only)

function sendText(socket, text) {
  const payload = Buffer.from(text, 'utf8');
  const n = payload.length;
  const header = n < 126 ? Buffer.from([0x81, n]) : n < 65536 ? Buffer.alloc(4) : Buffer.alloc(10);
  if (n >= 126 && n < 65536) {
    header[0] = 0x81;
    header[1] = 126;
    header.writeUInt16BE(n, 2);
  } else if (n >= 65536) {
    header[0] = 0x81;
    header[1] = 127;
    header.writeBigUInt64BE(BigInt(n), 2);
  }
  socket.write(Buffer.concat([header, payload]));
}

let nextConn = 1;

function acceptWebSocket(req, socket) {
  const key = req.headers['sec-websocket-key'];
  // While the host restarts its hello would be lost; refuse so the phone retries.
  if (!key || host === null) return socket.destroy();
  const accept = createHash('sha1').update(`${key}258EAFA5-E914-47DA-95CA-C5AB0DC85B11`).digest('base64');
  socket.write(
    'HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n' +
      `Sec-WebSocket-Accept: ${accept}\r\n\r\n`,
  );
  socket.setNoDelay(true);
  const id = nextConn++;
  sockets.set(id, socket);
  toHost(`O ${id}`);

  let buf = Buffer.alloc(0);
  socket.on('data', (chunk) => {
    buf = Buffer.concat([buf, chunk]);
    for (;;) {
      if (buf.length < 2) return;
      const opcode = buf[0] & 0x0f;
      const masked = (buf[1] & 0x80) !== 0;
      let len = buf[1] & 0x7f;
      let off = 2;
      if (len === 126) {
        if (buf.length < 4) return;
        len = buf.readUInt16BE(2);
        off = 4;
      } else if (len === 127) {
        if (buf.length < 10) return;
        len = Number(buf.readBigUInt64BE(2));
        off = 10;
      }
      const mask = masked ? buf.subarray(off, off + 4) : null;
      if (masked) off += 4;
      if (buf.length < off + len) return;
      const payload = Buffer.from(buf.subarray(off, off + len));
      buf = buf.subarray(off + len);
      if (mask) for (let i = 0; i < payload.length; i++) payload[i] ^= mask[i & 3];
      if (opcode === 0x1) toHost(`T ${id} ${payload.toString('utf8').replace(/\n/g, ' ')}`);
      else if (opcode === 0x8) socket.end(Buffer.from([0x88, 0x00]));
      else if (opcode === 0x9) socket.write(Buffer.concat([Buffer.from([0x8a, payload.length]), payload]));
    }
  });
  const gone = () => {
    if (sockets.get(id) !== socket) return;
    sockets.delete(id);
    toHost(`C ${id}`);
  };
  socket.on('close', gone);
  socket.on('error', gone);
}

// ------------------------------------------------------------ HTTP

function serveFile(res, name) {
  const path = join(dist, name);
  if (!existsSync(path)) {
    res.writeHead(404, { 'Content-Type': 'text/plain' });
    return res.end(`web/dist/${name} missing: run npm run build, or use npm run dev with PG_BOX`);
  }
  res.writeHead(200, { 'Content-Type': 'text/html; charset=utf-8', 'Cache-Control': 'no-cache' });
  res.end(readFileSync(path));
}

const server = createServer((req, res) => {
  const url = new URL(req.url ?? '/', 'http://sim');
  if (url.pathname === '/api/status') {
    res.writeHead(200, { 'Content-Type': 'application/json', 'Cache-Control': 'no-store' });
    return res.end(
      JSON.stringify({
        ssid: 'PartyGames-SIM',
        version: 'sim',
        uptimeMs: Date.now() - startedAt,
        resetReason: 1,
        heapFree: 0,
        heapMin: 0,
        psramFree: 0,
        psramTotal: 0,
        stations: sockets.size,
        wsClients: sockets.size,
      }),
    );
  }
  if (url.pathname === '/api/accept' && req.method === 'POST') {
    res.writeHead(204);
    return res.end();
  }
  if (url.pathname === '/portal') return serveFile(res, 'portal.html');
  if (url.pathname === '/') return serveFile(res, 'index.html');
  res.writeHead(404, { 'Content-Type': 'text/plain' });
  res.end('Not found');
});

server.on('upgrade', (req, socket) => {
  if (new URL(req.url ?? '/', 'http://sim').pathname === '/ws') acceptWebSocket(req, socket);
  else socket.destroy();
});

startHost();
server.listen(port, () => console.log(`sim: http://localhost:${port} (host ${hostBin})`));
