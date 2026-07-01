# Правила работы над проектом

## Описание проекта

- **Проект:** `Косарь` — гаражный RC mower build: Greenworks shell, hoverboard drive, ESP32, ELRS/CRSF, web dashboard, документация и будущая телеметрия.
- **Цель:** довести железо от bench-стенда до управляемой ходовой платформы, не теряя wiring, прошивочные решения, дизайн сайта и историю решений.
- **Аудитория:** Искандер, будущие агенты проекта, люди из build-in-public канала и те, кто захочет повторить сборку.
- **Ограничения:** не коммитить секреты, Wi-Fi credentials, локальные serial-port overrides и содержимое upstream checkout; публичный сайт остаётся read-only.

## Структура проекта

- `apps/kosar-dashboard/public/` — статический лендинг и mock dashboard.
- `firmware/esp32-crsf-web/` — ESP32 Arduino/PlatformIO firmware.
- `firmware/patches/` — сохранённые патчи для hoverboard firmware.
- `docs/` — прикладные docs: runbook, wiring, telemetry contract, visual style.
- `agent_docs/` — агентная документация по шаблону `2030ai/2030ai-project-template`.

## Команды

- `cd apps/kosar-dashboard/public && python3 -m http.server 8787` — локальный лендинг.
- `cd firmware/esp32-crsf-web && pio run` — сборка ESP32 firmware.
- `cd firmware/esp32-crsf-web && pio run -t upload` — прошивка ESP32.
- `cd firmware/esp32-crsf-web && pio device monitor` — serial monitor `115200`.

## Принципы работы

- Перед нетривиальными правками открыть `agent_docs/index.md`, затем только релевантные документы.
- Не двигать рабочие firmware/dashboard файлы ради чистоты структуры.
- Public dashboard не превращать в remote motor-control UI.
- Новые долгоживущие решения фиксировать в `agent_docs/development-history/`; важные архитектурные решения — в `agent_docs/adr/`.
- Для железа и прошивки давать конкретные пины, команды, проверки и последствия.

## Project-local skills

- Canonical source: `.agents/skills/<name>/SKILL.md`.
- Platform mirrors: `.claude/skills/<name>`, `.codex/skills/<name>`, `.cursor/skills/<name>` как symlink на `../../.agents/skills/<name>`.
- Slash-command файлы не добавлять; повторяемые workflow оформлять как skills.

## Стиль и безопасность репо

- JS: two-space indentation, `const`/`let`, small DOM-focused functions.
- C++ firmware: Arduino conventions, `static constexpr`, PascalCase structs, lowerCamelCase fields.
- Assets называть описательно: `kosar-dashboard-cockpit.webp`, `kosar-mvp-drive-greenpunk.webp`.
- Не читать и не коммитить `.env`, `wifi_local.h`, ключи, токены и приватные файлы.
- Если тесты/проверки не запускались — явно сказать почему.
