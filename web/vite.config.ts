import preact from '@preact/preset-vite';
import { defineConfig } from 'vite';
import { viteSingleFile } from 'vite-plugin-singlefile';

// One self-contained HTML file: a phone loads the whole UI over a single TCP
// connection, which matters with 15 phones and a small socket pool on the box.
export default defineConfig({
  plugins: [preact(), viteSingleFile()],
  build: { target: 'es2019' },
  // `npm run dev` while this PC is on the box's Wi-Fi talks to the real firmware.
  server: {
    proxy: {
      '/api': 'http://192.168.4.1',
      '/ws': { target: 'ws://192.168.4.1', ws: true },
    },
  },
});
