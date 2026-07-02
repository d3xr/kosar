# Архитектура

## Обзор

`Косарь` состоит из трёх рабочих контуров:

- ESP32 control/debug: PlatformIO firmware `firmware/esp32-crsf-web/`.
- Hoverboard drive: upstream `hoverboard-firmware-hack-FOC` + локальные patch-файлы в `firmware/patches/`.
- Hardware/docs: runbook, wiring, mechanical MVP и telemetry contract в `docs/`.

## Контекст

Проект started как garage RC bring-up. Сейчас важно не потерять связь между железом, прошивками и документацией. Репозиторий является source of truth для firmware, hardware docs и agent docs; Obsidian хранит долгий личный контекст проекта.

## Ключевые компоненты

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
ESP32
  -> local web UI
  -> future telemetry storage/export
```

Motor-control APIs live only in local firmware/debug context.

## Технологии и зависимости

- PlatformIO + Arduino framework for ESP32 firmware.
- OpenOCD/ST-Link for hoverboard controller flashing.
- GitHub repository `d3xr/kosar`.

## Нефункциональные требования и ограничения

- Не коммитить `wifi_local.h`, `.env`, serial overrides и private credentials.
- Не тащить `firmware/upstream/` в git.
- Не коммитить приватные web-витрины: публичный GitHub должен быть про firmware, dashboard/control и железо.
- Для firmware changes always preserve local stop/arm/timeout behaviour.

## Roadmap

- Стабилизировать ESP32 ↔ hoverboard UART telemetry.
- Подготовить telemetry export/storage для анализа заездов.
- Зафиксировать механический drive module после ходовых тестов.
