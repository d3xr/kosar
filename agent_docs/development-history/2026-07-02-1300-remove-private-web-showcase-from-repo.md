# [2026-07-02 13:00] Remove private web showcase from repo

Файл: `agent_docs/development-history/2026-07-02-1300-remove-private-web-showcase-from-repo.md`

## Что сделано

- Удалён приватный web-showcase слой со статической витриной, discovery-метой и generated images.
- Удалены docs, относящиеся только к приватной web-витрине.
- Удалены audit/history записи, которые описывали только витринный слой.
- README, AGENTS и `agent_docs` переписаны под новый scope: firmware, local dashboard/control, hoverboard patches, hardware docs.

## Зачем

GitHub должен быть про повторяемую сборку: dashboard/control, две прошивки, железо, wiring и runbook. Личная web-витрина не является частью публичного исходника.

## Обновлено

- [x] `README.md`
- [x] `AGENTS.md`
- [x] `agent_docs/architecture.md`
- [x] `agent_docs/index.md`
- [x] `agent_docs/glossary.md`
- [x] `docs/mechanical-mvp-drive-module.md`
- [x] приватный web-showcase слой удалён
- [ ] Тесты: markdownlint и grep-smoke перед коммитом

## Связанные решения

- `agent_docs/adr/2026-07-01-1531-adopt-2030ai-template.md`

## Следующие шаги

- Если нужен приватный исходник web-витрины, держать его вне публичного `d3xr/kosar`.
- Уточнить README под “две прошивки”: ESP32 firmware и hoverboard firmware patches/upstream workflow.
