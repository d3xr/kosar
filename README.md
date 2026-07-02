# Косарь

Гаражный RC-проект: Greenworks-корпус, донорский 10" гироскутер, RadioMaster Boxer ELRS, ESP32 и открытая документация сборки.

Дух проекта: **строим из того, что есть, но держим карту проводов, прошивок и решений чистой**.

## Кто я в этой вселенной

Codex тут не завлаб и не Scrum-мастер. Это бортовой инженер из мастерской:

- убирать туман;
- давать короткие следующие шаги;
- не превращать фановую сборку в диплом;
- фиксировать контекст в репо и Obsidian;
- останавливать только там, где реально можно сжечь железо или получить неуправляемый привод.

## Текущий квест

Гироскутерная плата прошита через ST-Link. ESP/Web UI/ELRS уже крутят моторы на стенде. Сейчас задача: **собрать жёсткий временный drive module, проверить управление, логи и первую ходовую механику**.

Текущая архитектура:

```text
RadioMaster Boxer / web debug
  -> ESP32 TTGO LoRa32
  -> UART 115200
  -> hoverboard mainboard EFeru VARIANT_USART
  -> два мотор-колеса
```

Нож Greenworks пока не существует. Он в другой серии.

## Что уже готово на Mac

Поставлено:

```bash
brew install openocd stlink platformio
```

Склонирована прошивка:

```bash
firmware/upstream/hoverboard-firmware-hack-FOC
```

Hoverboard-плата:

- MCU: `GD32F103RCT6`.
- SWD найден и припаян на гребёнку.
- ST-Link проверил связь: `Cortex-M3 detected`, target halt OK.
- Залита `VARIANT_USART`.
- Правый sideboard-разъём выбран как UART3.

ESP32-стенд:

- `firmware/esp32-crsf-web`
- Wi-Fi/web UI: `http://kosar.local/`
- домашний Wi-Fi локально в ignored `wifi_local.h`;
- CRSF вход ELRS: `GPIO16/17`;
- hoverboard UART: `GPIO25/26`;
- web debug control: можно рулить без пульта;
- Motor Test: левый/правый/оба мотора, 0..100%, forward/reverse;
- failsafe web heartbeat: 500 ms.
- debug cockpit: Drive, Motors, Channels, Hover, Logs, Network;
- web-control API: `POST + boot token`, случайный `GET` моторы не оживляет.

Патчи под первый UART-контур:

```bash
firmware/patches/stage1-safe-usart3-config.patch
```

## Главный документ

Вся практическая инструкция теперь одна:

[docs/garage-runbook.md](docs/garage-runbook.md)

Механический MVP:

[docs/mechanical-mvp-drive-module.md](docs/mechanical-mvp-drive-module.md)

## Что лежит в GitHub

- Dashboard/control: локальная веб-морда внутри [firmware/esp32-crsf-web](firmware/esp32-crsf-web).
- Прошивка 1: ESP32 bridge для ELRS/CRSF, web debug и UART hoverboard.
- Прошивка 2: hoverboard FOC `VARIANT_USART` workflow через upstream checkout и патчи в [firmware/patches](firmware/patches).
- Железо: runbook, wiring notes, mechanical MVP и telemetry contract в [docs](docs).

## Dashboard и telemetry

Telemetry contract:

[docs/telemetry-contract.md](docs/telemetry-contract.md)

Локальный dashboard живёт внутри ESP32-прошивки:

[firmware/esp32-crsf-web](firmware/esp32-crsf-web)

Он нужен для гаража: каналы, мотор-тест, hoverboard status, логи и network/debug.

## Agent docs

Проект приведён под `2030ai/2030ai-project-template`.

- [agent_docs/index.md](agent_docs/index.md) — карта агентной документации.
- [agent_docs/architecture.md](agent_docs/architecture.md) — архитектура проекта.
- [agent_docs/glossary.md](agent_docs/glossary.md) — словарь терминов.
- [agent_docs/development-history/](agent_docs/development-history/) — атомарная история итераций.
- [agent_docs/adr/](agent_docs/adr/) — значимые решения.

## Короткое правило

Если непонятно, куда паять, не паять. Сначала фото платы крупно, потом разметка, потом паяльник.
