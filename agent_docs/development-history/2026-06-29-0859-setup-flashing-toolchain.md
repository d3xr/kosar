# [2026-06-29 08:59] Setup flashing toolchain

Файл: `agent_docs/development-history/2026-06-29-0859-setup-flashing-toolchain.md`

## Что сделано

- Склонирован `EFeru/hoverboard-firmware-hack-FOC` в `firmware/upstream/`.
- Установлены `openocd`, `stlink`, `platformio`.
- Собран `VARIANT_PWM`.
- Добавлен safe patch для первого bench-теста.
- Добавлена короткая инструкция `docs/07-simple-flash-and-wiring.md`.

## Зачем

Свести первый этап к практическим действиям: собрать прошивку, подготовить ST-Link wiring и зафиксировать простой PWM-конфиг для ELRS PWM-приёмника.

## Обновлено

- [x] `docs/07-simple-flash-and-wiring.md`
- [x] `firmware/patches/stage1-safe-pwm-config.patch`
- [ ] Тесты: аппаратная прошивка не выполнялась, ST-Link ещё не подключён
- [x] Документация

## Связанные решения

- `docs/adr-0001-stage-1-control-path.md`

## Следующие шаги

- Подключить ST-Link к `GND/SWDIO/SWCLK`.
- Подтвердить pinout платы фото и мультиметром.
- Выполнить unlock/flash.
