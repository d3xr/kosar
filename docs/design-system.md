# Косарь — design system

Источник: Compass artifact `compass_artifact_wf-95970db1-41e9-44a6-ac8f-de0123e0ce01_text_markdown.md`.

## Направление

Рабочее имя: **Rustpunk Cozy / Косарьпанк**.

Система стоит на трёх слоях:

- `salvagepunk / junkpunk` — техника из доноров, хлама, стяжек, фанеры, текстолита и пайки;
- `cassette futurism` — CRT, терминал, моноширинные цифры, аналоговые приборы, рамки, тумблеры;
- `rural retrofuturism` — дача, мастерская, берёзы, тёплый свет, неглянцевая надежда.

Эмоция: инженерно, живо, намеренно несовершенно. Не стерильный sci-fi и не корпоративный cyberpunk.

## Design tokens

```css
:root {
  /* surfaces */
  --k-bg-0: #14100e;
  --k-bg-1: #1f1a17;
  --k-bg-2: #2a2320;
  --k-border: #4a4038;

  /* text */
  --k-text: #e8dcc8;
  --k-text-dim: #9a8c7a;

  /* brand */
  --k-rust: #c45508;
  --k-ember: #e8801a;
  --k-cyan: #3fb6b2;
  --k-verdigris: #2a6e6a;
  --k-brass: #b8893b;

  /* semantic */
  --k-ok: #1bff80;
  --k-warn: #ffb641;
  --k-alert: #e8801a;
  --k-crit: #ff4b33;
  --k-info: #2ecfff;

  /* type */
  --k-font-mono: "JetBrains Mono", "IBM Plex Mono", ui-monospace, SFMono-Regular, Menlo, Consolas, monospace;
  --k-font-head: "Russo One", "Oswald", system-ui, sans-serif;
  --k-font-body: "Oswald", system-ui, sans-serif;

  /* shape */
  --k-radius: 2px;
  --k-radius-panel: 4px;

  /* effects */
  --k-glow-rust: 0 0 8px rgba(196, 85, 8, .55);
  --k-glow-cyan: 0 0 8px rgba(63, 182, 178, .5);
  --k-glow-crit: 0 0 10px rgba(255, 75, 51, .7);
  --k-bevel: inset 1px 1px 0 rgba(255, 255, 255, .06),
              inset -1px -1px 0 rgba(0, 0, 0, .5);
}
```

## Цветовая семантика

- `--k-bg-*` — копоть, уголь, тёплый графит.
- `--k-rust / --k-ember` — бренд, энергия, лампа в мастерской.
- `--k-cyan / --k-info` — электроника, связь, телеметрия.
- `--k-ok` — нормальное состояние.
- `--k-warn` — внимание.
- `--k-alert` — предупреждение.
- `--k-crit` — критическое состояние.
- `--k-brass` — механика, границы, вторичный металл.

## Типографика

- H1/бренд: `Russo One`.
- H2/H3 и крупные интерфейсные заголовки: `Russo One` или `Oswald`.
- Данные, логи, каналы, пины, команды: `JetBrains Mono`.
- Body: `Oswald` с fallback на system-ui.
- Декоративные CRT-шрифты без кириллицы использовать только для латинских декоративных меток.

## UI правила

- Данные читаются первыми, грязь и CRT вторыми.
- Scanline/grain не кладём поверх цифр и логов; декоративные эффекты держать под контентом или на hero/artifact.
- Glow только на ключевых индикаторах, активных чипах и тревожных состояниях.
- Панели — жёсткая сетка, малый радиус, bevel, угловатая геометрия.
- Dashboard — приборная панель: gauges, bars, terminal, status chips.
- Landing — история сборки, сильный hero, build steps, artifact и прямые ссылки.
- Public site остаётся read-only.

## Материалы и графика

- ржавчина, облупленная краска, латунь, зелёный PCB, фанера, изолента, стяжки;
- тёплый янтарный свет + холодный циан;
- generated hero/artifact images обязательны: без графики проект теряет характер.

## Анти-тон

- глянцевый корпоративный sci-fi;
- холодный сине-фиолетовый cyberpunk;
- стерильная нержавейка/Apple future;
- warning-copy ради warning-copy;
- legal-блоки вместо архитектурной границы;
- публичные remote motor-control promises.

## AI image prompt tail

```text
warm rusty retrofuturism, salvagepunk, built from scrap and duct tape, cozy dystopian, amber and cyan neon glow on dark grimy background, halation glow, lived-in used-future, rural cyberpunk village mood, cassette futurism CRT tech, cinematic volumetric light, film grain, painterly, highly detailed
```

Negative:

```text
clean stainless steel, corporate, sleek, sterile, cold blue cyberpunk, glossy plastic, minimalist white studio, neon pink overload, chrome
```

## Implementation status

Current website uses these tokens in `apps/kosar-dashboard/public/src/styles.css`. Legacy aliases `--bg`, `--card`, `--line`, `--text`, `--muted`, `--rust`, `--ember`, `--cyan`, `--ok`, `--warn`, `--crit` remain as compatibility aliases over `--k-*`.
