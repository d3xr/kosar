# [2026-07-01 18:22] Design system compliance

Файл: `agent_docs/audits/2026-07-01-1822-design-system-compliance.md`

## Scope

Проверены:

- Compass artifact `compass_artifact_wf-95970db1-41e9-44a6-ac8f-de0123e0ce01_text_markdown.md`;
- `docs/visual-style.md`;
- `docs/landing-design-code.md`;
- `apps/kosar-dashboard/public/index.html`;
- `apps/kosar-dashboard/public/src/styles.css`.

## Verdict

`WARN`, не `PASS`.

Сайт уже попадает в Rustpunk Cozy по палитре, графике и структуре, но до прохода были три несоответствия:

- tokens не были вынесены как `--k-*`;
- шрифты были системными, без Russo/Oswald/JetBrains слоя;
- глобальный scanline overlay лежал поверх всего контента.

## Исправлено

- Добавлен `docs/design-system.md`.
- CSS переведён на `--k-*` tokens с legacy aliases.
- Добавлены Google Fonts: `Russo One`, `Oswald`, `JetBrains Mono`.
- Заголовки переведены на display/head font, данные и логи — на mono.
- Global scanline overlay опущен под контент и ослаблен.
- `panel--warning` заменён на `panel--boundary`.

## PASS

- Палитра соответствует primary Rustpunk Cozy.
- Есть amber/cyan контраст.
- Есть generated visual assets и hero/artifact.
- Dashboard read-only.
- Layout строится на panels, gauges, bars, terminal.
- Copy не использует legal/warning как основной мотив.

## WARN

- `augmented-ui` не подключён: текущие bevel/panel эффекты сделаны CSS-only.
- Нет скевоморфного analog gauge; gauges пока цифровые.
- Нет иконочного набора Lucide/Tabler.
- `body` использует `Oswald`; для длинных future docs/pages может понадобиться отдельный body font.

## CRITICAL_FAIL

Нет.

## Следующие шаги

- Если cockpit станет полноценным приложением, добавить icon system и один analog gauge.
- Для будущих generated assets держать prompt tail из `docs/design-system.md`.
- После визуальных правок переснимать screenshot desktop/mobile.
