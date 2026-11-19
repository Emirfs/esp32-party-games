// Gzips the pages the firmware embeds (see firmware/src/CMakeLists.txt).
import { readFileSync, writeFileSync } from 'node:fs';
import { gzipSync } from 'node:zlib';

for (const name of ['index.html', 'portal.html']) {
  const src = readFileSync(new URL(`../dist/${name}`, import.meta.url));
  const gz = gzipSync(src, { level: 9 });
  writeFileSync(new URL(`../dist/${name}.gz`, import.meta.url), gz);
  console.log(`${name}: ${src.length} -> ${gz.length} bytes gzip`);
}
