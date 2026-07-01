# [2026-07-01 18:22] Extract design system

Файл: `agent_docs/development-history/2026-07-01-1822-extract-design-system.md`

## Что сделано

- Compass artifact разобран в отдельный документ `docs/design-system.md`.
- `docs/visual-style.md` и `docs/landing-design-code.md` связаны с полной дизайн-системой.
- `apps/kosar-dashboard/public/src/styles.css` приведён к `--k-*` tokens.
- `apps/kosar-dashboard/public/index.html` получил шрифтовой слой Russo/Oswald/JetBrains.
- Добавлен compliance audit `agent_docs/audits/2026-07-01-1822-design-system-compliance.md`.

## Зачем

Нужно отделить дизайн-систему от переписки и Compass-артефакта, чтобы проект не терял визуальные правила при следующих итерациях сайта, dashboard и generated art.

## Обновлено

- [x] `docs/design-system.md`
- [x] `docs/visual-style.md`
- [x] `docs/landing-design-code.md`
- [x] `agent_docs/audits/2026-07-01-1822-design-system-compliance.md`
- [x] Тесты: markdownlint запущен, smoke для runtime не нужен — правки статические.

## Связанные решения

- `agent_docs/adr/2026-07-01-1531-adopt-2030ai-template.md`

## Следующие шаги

- После cockpit-редизайна добавить icon system и analog gauge.
