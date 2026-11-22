import preact from '@preact/preset-vite';
import { defineConfig } from 'vite';
import { viteSingleFile } from 'vite-plugin-singlefile';

// Only this env lookup needs Node's `process`; not worth @types/node.
declare const process: { env: Record<string, string | undefined> };

// `npm run dev` talks to the real box while this PC is on its Wi-Fi, or to the
// host simulator with PG_BOX=http://localhost:7420 (`npm run sim`).
const box = process.env.PG_BOX || 'http://192.168.4.1';

// One self-contained HTML file: a phone loads the whole UI over a single TCP
// connection, which matters with 15 phones and a small socket pool on the box.
export default defineConfig({
  plugins: [preact(), viteSingleFile()],
  build: { target: 'es2019' },
  server: {
    proxy: {
      '/api': box,
      '/ws': { target: box.replace(/^http/, 'ws'), ws: true },
    },
  },
});
