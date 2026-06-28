# Архитектура

## Обзор

`Косарь` строится как RC-first тяговая платформа:

```text
RadioMaster Boxer ELRS
  -> ELRS receiver / ESP32 bridge
  -> EFeru hoverboard mainboard
  -> два 10" hub motor wheels
  -> Greenworks chassis
```

Stage 1 не включает нож и автономию. Цель — доказать управляемое движение, остановку и safety gates.

## Контекст

- Донор ходовой: 10" гироскутер.
- Mainboard: предположительно Gen1 с `GD32F103RCT6`.
- Прошивка: `EFeru/hoverboard-firmware-hack-FOC`.
- Пульт: RadioMaster Boxer ELRS.
- Потенциальный bridge: ESP32 для `CRSF -> EFeru UART`.
- Greenworks 48V используется только как механическая база на Stage 1; силовой контур ножа отключен.

## Ключевые компоненты

- **RC link:** ELRS; прямой MVP путь — PWM receiver, fallback — ESP32 CRSF bridge.
- **Motor controller:** родная hoverboard board на EFeru FOC.
- **Drive power:** родная 10S батарея гироскутера.
- **Safety:** e-stop, receiver failsafe, EFeru timeout, физически отключенный нож.
- **Docs:** `docs/` для инженерных runbooks, `agent_docs/` для agent process/ADR/history.

- Project-local skills source: `.agents/skills/<name>/SKILL.md`.
- Platform skill mirrors: `.claude/skills/`, `.codex/skills/`, `.cursor/skills/`.

## Потоки данных

```text
PWM path:
Boxer -> ELRS PWM RX -> EFeru VARIANT_PWM -> motor mix -> left/right motors

ESP32 path:
Boxer -> ELRS CRSF RX -> ESP32 -> EFeru VARIANT_USART -> motor mix -> left/right motors
```

## Технологии и зависимости

- EFeru FOC upstream checkout не коммитится: `firmware/upstream/`.
- Для прошивки нужны `openocd`, `st-flash`, `platformio/pio` или `arm-none-eabi-gcc`.
- Для будущего ESP32 bridge кандидат: `AlfredoSystems/AlfredoCRSF`.

## Нефункциональные требования и ограничения

- Greenworks 48V не подключать к hoverboard board.
- До ground test должны быть доказаны e-stop и failsafe.
- Sideboard cable прозванивать мультиметром до подключения RX/ESP32.
- Не включать `TANK_STEERING` на MVP.
- Не включать нож в Stage 1.

## Roadmap

1. Stage 1: прошивка и RC движение на родной платформе.
2. Stage 1.5: перенос ходовой на Greenworks без ножа.
3. Stage 2: отдельный safety design режущего узла.
4. Stage 3+: автономия/RTK/ROS только после стабильной ходовой.
