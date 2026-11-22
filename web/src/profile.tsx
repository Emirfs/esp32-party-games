import { useState } from 'preact/hooks';
import { t } from './i18n';
import { COLORS, useApp } from './ui';

const AVATARS = [
  '🦊', '🐼', '🐸', '🐙', '🦁', '🐯', '🐨', '🐵', '🦉', '🐧', '🐢', '🦄',
  '🐝', '🦋', '🐳', '🦖', '🍕', '🌵', '🍄', '🌙', '⭐', '🔥', '🎸', '🤖',
];
const NAME_MAX = 16;

/** Create the profile (me == -1) or edit it from the lobby (`onDone` set). */
export function ProfileScreen({ onDone }: { onDone?: () => void }) {
  const { view, send } = useApp();
  const p = view.profile;
  const [name, setName] = useState(p?.name ?? '');
  const [avatar, setAvatar] = useState(p?.avatar ?? AVATARS[Math.floor(Math.random() * AVATARS.length)]);
  const [color, setColor] = useState(p?.color ?? Math.floor(Math.random() * COLORS.length));
  const trimmed = name.trim();
  const joining = view.me < 0;
  const blocked = joining && view.full;
  const avatars = AVATARS.includes(avatar) ? AVATARS : [avatar, ...AVATARS];

  const submit = (e: Event) => {
    e.preventDefault();
    if (!trimmed || blocked) return;
    send({ t: 'profile', name: trimmed, avatar, color });
    onDone?.();
  };

  return (
    <form class="card stack" onSubmit={submit}>
      <h2>{joining ? t('profileTitle') : t('editProfile')}</h2>
      <div class="preview">
        <span class="avatar big" style={{ background: COLORS[color] }}>
          {avatar}
        </span>
        <strong>{trimmed || '…'}</strong>
      </div>
      <label class="field">
        <span>{t('name')}</span>
        <input
          value={name}
          maxLength={NAME_MAX}
          placeholder={t('namePlaceholder')}
          autoComplete="nickname"
          enterKeyHint="done"
          onInput={(e) => setName([...e.currentTarget.value].slice(0, NAME_MAX).join(''))}
        />
      </label>
      <fieldset>
        <legend>{t('avatar')}</legend>
        <div class="emoji-grid">
          {avatars.map((a) => (
            <button type="button" key={a} aria-pressed={a === avatar} onClick={() => setAvatar(a)}>
              {a}
            </button>
          ))}
        </div>
      </fieldset>
      <fieldset>
        <legend>{t('color')}</legend>
        <div class="color-grid">
          {COLORS.map((c, i) => (
            <button type="button" key={c} aria-pressed={i === color} aria-label={`${t('color')} ${i + 1}`} style={{ background: c }} onClick={() => setColor(i)} />
          ))}
        </div>
      </fieldset>
      {blocked && <p class="notice">{t('roomFull')}</p>}
      <button class="primary" type="submit" disabled={!trimmed || blocked}>
        {joining ? t('join') : t('save')}
      </button>
      {onDone && (
        <button type="button" class="secondary" onClick={onDone}>
          {t('back')}
        </button>
      )}
    </form>
  );
}
