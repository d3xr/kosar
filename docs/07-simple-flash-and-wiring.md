# Простая инструкция: прошивка и первый RC-тест

## Что уже готово на Mac

Установлено:

```bash
brew install openocd stlink platformio
```

Проверено:

```bash
openocd --version
st-flash --version
pio --version
```

Прошивка лежит тут:

```bash
firmware/upstream/hoverboard-firmware-hack-FOC
```

Собранный файл для первого теста:

```bash
firmware/upstream/hoverboard-firmware-hack-FOC/.pio/build/VARIANT_PWM/firmware.bin
```

## Если надо пересобрать

```bash
cd /Users/d3xr/Documents/Косарь/firmware/upstream/hoverboard-firmware-hack-FOC
git apply /Users/d3xr/Documents/Косарь/firmware/patches/stage1-safe-pwm-config.patch
pio run -e VARIANT_PWM
```

`VARIANT_PWM` выбран как самый простой путь для ELRS PWM-приёмника.

## Что припаивать для прошивки

К ST-Link нужны только 3 провода:

| ST-Link | Плата гироскутера |
|---|---|
| `GND` | `GND` |
| `SWDIO` | `SWDIO` |
| `SWCLK` | `SWCLK` |

Не подключать:

- `3.3V` от ST-Link;
- `5V` от ST-Link;
- Greenworks 48V.

Плату гироскутера питать её родной батареей. Во время unlock/flash держать кнопку питания на плате/гироскутере, иначе плата может выключиться.

`NRST` обычно не нужен. Если ST-Link не увидит MCU, тогда добавим `NRST`.

Паять навсегда не обязательно. Нормально временно припаять 3 тонких провода к площадкам или использовать pogo pins/прищепку, если будет удобно.

## Команды прошивки

Сначала unlock. Это стирает родную прошивку:

```bash
cd /Users/d3xr/Documents/Косарь/firmware/upstream/hoverboard-firmware-hack-FOC
openocd -f interface/stlink-v2.cfg -f target/stm32f1x.cfg -c init -c "reset halt" -c "stm32f1x unlock 0"
```

Потом прошивка:

```bash
pio run -e VARIANT_PWM -t upload
```

Если `pio upload` будет капризничать, запасная команда:

```bash
st-flash --reset write .pio/build/VARIANT_PWM/firmware.bin 0x8000000
```

## Что подключать для RC

Это только если приёмник ELRS с PWM-выходами.

EFeru `VARIANT_PWM` ждёт:

- `CH1 = steering`;
- `CH2 = speed`;
- вход на правый sideboard-разъём платы гироскутера;
- в MCU это `PB10` для CH1 и `PB11` для CH2.

По цветам проводов не верить. Перед подключением мультиметром найти:

- реальный `GND`;
- нет ли 12/15V на красном проводе sideboard-разъёма.

Приёмник питать от отдельного нормального 5V BEC/DC-DC. Не питать приёмник от sideboard-разъёма, пока не проверено напряжение.

## Настройки пульта/приёмника

Минимум:

- CH1 steering: центр `1500us`, failsafe `1500us`.
- CH2 speed: центр `1500us`, failsafe `1500us`.
- PWM output: 50 Hz или 100 Hz.
- Отдельный тумблер ARM.
- Скорость лучше на self-centering stick/ось, не на нецентрирующийся газ.

CH3 не использовать как speed без настройки failsafe: у некоторых ELRS PWM RX дефолт CH3 может уйти в `988us`, а это не стоп.

## Первый запуск

1. Нож Greenworks не подключён.
2. Колёса в воздухе.
3. Питание платы лучше через лабораторник 36-42V с лимитом 1-2A, если есть.
4. Включить пульт, проверить центр каналов.
5. Подать совсем малый газ.
6. Проверить: вперёд, назад, влево, вправо.
7. Выключить пульт: моторы должны остановиться.

Если мотор дёргается, гудит или плата греется — сразу стоп.

## Текущий безопасный конфиг

В `Inc/config.h` сейчас локально выставлено:

```c
#define I_MOT_MAX       8
#define I_DC_MAX        10
#define N_MOT_MAX       60
#define DEFAULT_RATE    160
```

В `VARIANT_PWM`:

```c
#define SPEED_COEFFICIENT 8192
#define STEER_COEFFICIENT 8192
```

Это медленный bench-профиль. После успешного теста можно будет поднять скорость.
