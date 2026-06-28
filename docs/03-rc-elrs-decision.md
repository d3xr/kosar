# RC / ELRS Decision

Исходные данные:

- Передатчик: RadioMaster Boxer ELRS.
- Приемники: есть несколько штук, точные модели пока не зафиксированы.
- В наличии: ESP32/Arduino, пайка и отладка доступны.
- EFeru напрямую поддерживает PPM/PWM/iBUS/UART, но не CRSF.

## Вывод

ELRS обычно говорит с полетным контроллером по CRSF. Поэтому "ELRS receiver напрямую в EFeru" работает только если конкретный приемник умеет выдать PWM или PPM/iBUS через отдельный режим/конвертер.

## Вариант A. Direct PWM

Использовать, если приемник имеет PWM outputs.

Схема:

```text
RadioMaster Boxer ELRS
  -> ELRS PWM receiver
  -> EFeru VARIANT_PWM on right sensor cable
  -> hoverboard mainboard
```

Плюсы:

- минимальный код;
- быстро проверить движение;
- failsafe можно проверять как потерю PWM pulses.

Минусы:

- нужен именно PWM receiver или PWM breakout;
- меньше места для логики arming/rate-limit/telemetry.

Verdict: `PASS`, если приемник с PWM реально есть.

Настройки для первого запуска:

- Output mode: PWM 50Hz или 100Hz.
- CH1 steering: failsafe `1500us`.
- CH2 speed: failsafe `1500us`.
- Не использовать CH3 как speed без явной перенастройки: у ELRS PWM receiver дефолтная failsafe-позиция для output 3 может быть около `988us`, а для тележки нужен нейтральный стоп `1500us`.
- Отдельный ARM switch обязателен, но он не заменяет нейтральный failsafe.

## Вариант B. Direct PPM / iBUS

Использовать только если приемник точно умеет PPM Sum или iBUS.

PPM:

- `VARIANT_PPM`;
- right sensor cable;
- CH1 steering, CH2 speed.

iBUS:

- `VARIANT_IBUS`;
- FlySky iBUS, не CRSF.

Verdict: `WARN`, потому что для ELRS это не дефолтный путь.

## Вариант C. ESP32 CRSF -> EFeru UART

Схема:

```text
RadioMaster Boxer ELRS
  -> ELRS serial receiver, CRSF
  -> ESP32
  -> EFeru VARIANT_USART on USART3/right cable
  -> hoverboard mainboard
```

ESP32 делает:

- читает CRSF channels;
- требует arming switch;
- мапит throttle/steering в `-1000..1000`;
- режет скорость и ускорение;
- при потере свежего CRSF пакета отправляет `0,0`;
- при аварии перестает слать команды, чтобы EFeru serial timeout тоже остановил моторы.

Референс библиотеки: https://github.com/AlfredoSystems/AlfredoCRSF

Verdict: `PASS` как основной путь, если приемники только serial CRSF.

## Рекомендация для первой сессии с железом

1. Записать точные модели приемников.
2. Если один из них PWM - начать с `VARIANT_PWM`.
3. Если все приемники serial CRSF - не тратить время на PPM/iBUS, делать ESP32 bridge.
4. До любого ground test доказать failsafe:
   - выключение пульта;
   - потеря питания приемника/ESP32;
   - отвал signal wire;
   - throttle не в нуле при включении.

## Каналы

Минимальная раскладка:

- CH1 steering.
- CH2 throttle.
- CH5 arm/disarm.
- CH6 speed mode: bench / slow / normal.

Для MVP нормальным считается только `bench` и `slow`; `normal` появится после тестов на земле.

Для RadioMaster Boxer скорость лучше вешать на self-centering ось или канал с возвратом в центр. Нецентрирующийся throttle-stick опасен для тележки: после arming он может оставить ненулевую команду.

## Скорость

Для 10" колеса ориентиры:

- 60 rpm: около 2.9 км/ч.
- 80 rpm: около 3.8 км/ч.

Стартовый профиль:

- `N_MOT_MAX = 40-60 rpm` для bench/первой земли.
- Потом `80-120 rpm`, если нет перегрева и рывков.
- `FIELD_WEAK_ENA = 0`.
- Начать с `VLT_MODE`; `SPD_MODE` тестировать позже, потому что на траве он будет активнее добирать момент.
