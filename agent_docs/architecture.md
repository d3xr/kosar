# Архитектура

## Обзор

`Косарь` состоит из трёх рабочих контуров:

- Public landing/dashboard: статический сайт `apps/kosar-dashboard/public/`.
- ESP32 control/debug: PlatformIO firmware `firmware/esp32-crsf-web/`.
- Hoverboard drive: upstream `hoverboard-firmware-hack-FOC` + локальные patch-файлы в `firmware/patches/`.

## Контекст

Проект started как garage RC bring-up. Сейчас важно не потерять связь между железом, прошивками, сайтом и документацией. Репозиторий является source of truth для кода, public docs и agent docs; Obsidian хранит долгий личный контекст проекта.

## Ключевые компоненты

- `apps/kosar-dashboard/public/index.html` — лендинг и mock/replay dashboard для `kosar.vyroslo.ru`.
- `apps/kosar-dashboard/public/src/app.js` — демо-поток каналов и лога.
- `apps/kosar-dashboard/public/src/styles.css` — Rustpunk Cozy визуальный слой.
- `firmware/esp32-crsf-web/src/main.cpp` — CRSF reader, web UI/API, hoverboard UART bridge.
- `firmware/patches/stage1-safe-usart3-config.patch` — UART3 hoverboard patch.
- `docs/garage-runbook.md` — operational runbook.
- `docs/telemetry-contract.md` — будущий серверный контракт телеметрии.

## Потоки данных

```text
RadioMaster Boxer
  -> ELRS receiver
  -> ESP32 CRSF parser
  -> UART command 115200
  -> hoverboard controller VARIANT_USART
  -> motor wheels
```

```text
ESP32 / mock replay
  -> local web UI or future telemetry server
  -> public landing dashboard
```

Public site remains read-only. Motor-control APIs live only in local firmware/debug context.

## Технологии и зависимости

- Static HTML/CSS/JS for public landing.
- PlatformIO + Arduino framework for ESP32 firmware.
- OpenOCD/ST-Link for hoverboard controller flashing.
- Nginx + TLS on Timeweb VPS for `kosar.vyroslo.ru`.
- GitHub repository `d3xr/kosar`.

## Нефункциональные требования и ограничения

- Не коммитить `wifi_local.h`, `.env`, serial overrides и private credentials.
- Не тащить `firmware/upstream/` в git.
- Для public landing не добавлять claims про OAuth/API/MCP/production telemetry, пока этого нет.
- Для firmware changes always preserve local stop/arm/timeout behaviour.

## Roadmap

- Стабилизировать ESP32 ↔ hoverboard UART telemetry.
- Разделить website story и electronics workflow по чатам/докам.
- Подготовить server telemetry ingestion для read-only status.
- Зафиксировать механический drive module после ходовых тестов.
