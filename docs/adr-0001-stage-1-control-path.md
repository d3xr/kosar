# ADR-0001: Stage 1 Control Path

Дата: 2026-06-28.

## Контекст

Нужно быстро получить управляемую RC-платформу на мотор-колесах гироскутера. Передатчик RadioMaster Boxer работает на ELRS. Плата гироскутера предположительно Gen1 с `GD32F103RCT6`, что подходит под EFeru FOC.

## Решение

Использовать `EFeru/hoverboard-firmware-hack-FOC` как основную прошивку Stage 1.

Путь управления выбирать по фактическому приемнику:

1. Если есть ELRS receiver с PWM outputs: `VARIANT_PWM` напрямую в правый sensor cable.
2. Если receiver умеет PPM Sum: `VARIANT_PPM` напрямую в правый sensor cable.
3. Если receiver только CRSF: ESP32 bridge `CRSF -> EFeru VARIANT_USART`.

Нож Greenworks исключен из Stage 1 и физически отключен.

`TANK_STEERING` в EFeru не включать на MVP: управляем одной скоростью и одним рулением, микшер оставляем внутри прошивки.

## Не делаем

- Не подключаем Greenworks 48V к hoverboard board.
- Не делаем автономию.
- Не включаем нож по RC.
- Не тащим OpenMower/Sunray в MVP.
- Не вендорим чужие firmware repos до license review.

## Последствия

Плюсы:

- Быстрый bench-test ходовой.
- Сохраняется путь к ESP32/автономии.
- Риски ножа и высоковольтной Greenworks системы не входят в MVP.

Минусы:

- ELRS CRSF требует bridge, если нет PWM/PPM-capable receiver.
- Без фактического pinout платы нельзя считать конфиг финальным.
- Failsafe должен быть доказан физическим тестом.
