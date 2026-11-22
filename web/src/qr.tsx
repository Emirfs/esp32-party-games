import { encode } from 'uqr';

/** A QR code as one SVG path (dark modules on white, quiet zone included). */
export function Qr({ text, label }: { text: string; label: string }) {
  const { data, size } = encode(text, { ecc: 'M', border: 2 });
  let d = '';
  data.forEach((row, y) =>
    row.forEach((dark, x) => {
      if (dark) d += `M${x} ${y}h1v1h-1z`;
    }),
  );
  return (
    <figure class="qr">
      <svg viewBox={`0 0 ${size} ${size}`} role="img" aria-label={label} shape-rendering="crispEdges">
        <rect width={size} height={size} fill="#fff" />
        <path d={d} fill="#000" />
      </svg>
      <figcaption>{label}</figcaption>
    </figure>
  );
}

/** Wi-Fi join string; `\ ; , : "` are escaped as the format requires. */
export function wifiQrText(ssid: string, pass: string): string {
  const esc = (s: string) => s.replace(/([\\;,:"])/g, '\\$1');
  return `WIFI:T:WPA;S:${esc(ssid)};P:${esc(pass)};;`;
}
