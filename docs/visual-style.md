# Косарь — visual style

Практический стиль проекта: **Rustpunk Cozy / Косарьпанк**.

## ДНК

- salvagepunk: железо из доноров, стяжки, пайка, фанера, ржавчина;
- cassette futurism: CRT, терминал, моноширинные цифры, приборная сетка;
- rural retrofuturism: дача, мастерская, берёзы, тёплый свет;
- не глянец, не стерильный sci-fi, не корпоративная SaaS-морда.

## Цвета

```css
--bg:    #14100e;
--card:  #2a2320;
--line:  #4a4038;
--text:  #e8dcc8;
--muted: #9a8c7a;
--rust:  #c45508;
--ember: #e8801a;
--cyan:  #3fb6b2;
--ok:    #1bff80;
--warn:  #ffb641;
--crit:  #ff4b33;
```

Смысл:

- amber/rust — тепло, мастерская, энергия;
- cyan/patina — телеметрия, электроника, связь;
- green phosphor — OK;
- red/orange — danger/safety.

## UI правила

- данные читаются первыми, грязь и CRT-эффекты вторыми;
- публичный dashboard read-only;
- опасные команды не выглядят как обычные CTA;
- никакого public joystick;
- текст поверх картинок только с тёмным safety-overlay;
- картинки обязательны: без графики проект мёртв.

## Assets

Сгенерированные изображения лежат в:

```text
apps/kosar-dashboard/public/assets/
```

Основные:

- `kosar-hero-rustpunk.webp` — первый экран;
- `kosar-build-story.webp` — гаражная история;
- `kosar-dashboard-cockpit.webp` — telemetry cockpit;
- `kosar-hardware-closeup.webp` — железо и проводка.
- `kosar-mvp-drive-art.webp` — MVP drive module как rustpunk artifact вместо школьной схемы.

PNG-исходники оставлены рядом как source-grade assets.
