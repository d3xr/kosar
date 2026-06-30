# Гаражный runbook: оживляем гироскутер

## Сцена 0. Где мы сейчас

Есть:

- разобранный гироскутер;
- плата `Yiyun/YK97`-класса с `GD32F103RCT6`;
- два мотор-колеса;
- родная батарея гироскутера;
- ST-Link V2;
- TTGO LoRa32 ESP32;
- ELRS receiver;
- RadioMaster Boxer, сейчас может быть разряжен;
- веб-управление как запасной debug-driver.

Цель: **колёса крутятся от пульта, пока висят в воздухе**.

Нож Greenworks не подключать. Greenworks 48V не смешивать с hoverboard 10S.

## Сцена 1. Что уже сделано

Hoverboard:

- SWD-гребёнка припаяна.
- ST-Link увидел плату:
  ```text
  Target voltage: 3.297196
  Cortex-M3 r2p1 processor detected
  target halted
  ```
- `VARIANT_USART` залит успешно.

ESP32:

- web UI работает на `http://kosar.local/`;
- CRSF с ELRS receiver читается;
- каналы видны в вебморде;
- добавлен web debug control;
- добавлена отправка EFeru serial-команд в hoverboard;
- добавлено чтение hoverboard feedback.

## Сцена 2. Пины и провода

### SWD прошивка hoverboard

На SWD-гребёнке платы:

```text
3.3V   не подключать
SWCLK  -> ST-Link SWCLK / TCK
GND    -> ST-Link GND
SWDIO  -> ST-Link SWDIO / TMS
```

ST-Link питание не даёт:

```text
3.3V -> не подключать
5V   -> не подключать
RST  -> пока не нужен
```

Плата питается от родной батареи. При прошивке держать кнопку питания.

### ESP32 -> ELRS receiver

```text
TTGO 5V/VIN -> ELRS 5V
TTGO GND    -> ELRS GND
TTGO GPIO16 -> ELRS TX
TTGO GPIO17 -> ELRS RX, later telemetry
```

### ESP32 -> hoverboard right sideboard

Правый sideboard = `USART3`.

```text
TTGO GPIO25 -> hoverboard PB11 / RX
TTGO GPIO26 -> hoverboard PB10 / TX
TTGO GND    -> hoverboard GND
```

`15V 200mA` с sideboard не использовать для ESP32.

### Питание ESP32 на борту

Для стола:

```text
ESP от USB или powerbank
hoverboard от родной батареи
GND ESP и hoverboard общий
```

Для борта:

```text
10S battery -> buck 5V 2A/3A -> TTGO 5V/VIN + ELRS 5V
```

## Сцена 3. Hoverboard firmware

Используем `EFeru/hoverboard-firmware-hack-FOC`.

Режим:

```text
VARIANT_USART
FOC_CTRL + VLT_MODE
right sideboard USART3
baud 115200
```

Безопасные лимиты:

```c
#define I_MOT_MAX       8
#define I_DC_MAX        10
#define N_MOT_MAX       60
#define DEFAULT_RATE    160
```

Патч:

```text
firmware/patches/stage1-safe-usart3-config.patch
```

Команды, если понадобится повторить:

```bash
cd /Users/d3xr/Documents/Косарь/firmware/upstream/hoverboard-firmware-hack-FOC

openocd -f interface/stlink.cfg -f target/stm32f1x.cfg -c "init; reset halt; targets; exit"

pio run -e VARIANT_USART -t upload
```

## Сцена 4. ESP32 firmware

Проект:

```text
firmware/esp32-crsf-web
```

Что умеет:

- читает CRSF с ELRS receiver;
- показывает каналы в вебморде;
- хранит подписи каналов в browser `localStorage`;
- считает `speed/steer`;
- режет управление через `Arm`;
- даёт профили `turtle/normal/full`;
- шлёт hoverboard serial command;
- читает hoverboard feedback;
- даёт web debug control без пульта.

Прошить:

```bash
cd /Users/d3xr/Documents/Косарь/firmware/esp32-crsf-web
pio run -t upload
```

Веб:

```text
http://kosar.local/

fallback AP:
Wi-Fi: Kosar-RC
pass: kosar1234
url: http://192.168.4.1/
```

## Сцена 5. Управление

```text
CH1 Roll  -> steer
CH2 Pitch -> speed
CH5 Arm   -> gate движения
CH6 Mode  -> профиль отклика
```

Профили:

```text
turtle: max 100, accel 120/s, steer 180/s
normal: max 240, accel 420/s, steer 520/s
full:   max 420, accel 1200/s, steer 1200/s
```

`Throttle` не используем как газ: он не самоцентрируется.

EFeru serial protocol:

```text
start    = 0xABCD
steer    = -1000..1000
speed    = -1000..1000
checksum = start ^ steer ^ speed
```

## Сцена 6. Web debug mode

Нужен, когда пульт разряжен или ELRS не готов.

Веб-блок:

```text
web drive  -> включает web-source
web arm    -> разрешает движение
profile    -> turtle / normal / full
speed      -> вперёд/назад
steer      -> поворот
STOP       -> speed=0, steer=0, web arm off
source     -> rc/web
```

Failsafe:

```text
если браузер не шлёт heartbeat > 500 ms -> команда 0
если web drive выключен -> команда 0
если web arm off -> команда 0
```

## Сцена 7. Первый живой тест

Порядок:

1. колёса в воздухе;
2. hoverboard от родной батареи;
3. ESP от USB или powerbank;
4. общий GND между ESP и hoverboard;
5. открыть `http://kosar.local/`;
6. проверить `hoverboard OK`;
7. `profile = turtle`;
8. `web drive ON`;
9. `web arm ON`;
10. `speed = 5..10`;
11. нажать `STOP`.

Стоп-триггеры:

- мотор дёргается и не крутится нормально;
- плата быстро греется;
- запах горячей электроники;
- при выключении пульта колёса не стопятся.

Тогда питание вниз, паяльник в сторону, разбираем спокойно.

## Сцена 8. Потом на косилку

Не сразу в корпус Greenworks. Сначала отдельный приводной модуль:

```text
2 мотор-колеса
рама/пластина
hoverboard controller в боксе
ESP + ELRS + buck в боксе
родная hoverboard 10S battery
механический kill switch
```

Потом крепим модуль к косилке. Нож не включать до отдельного safety-gate.

## Сцена 9. Отложенные ништяки

Напряжение батареи:

- сейчас не блокирует первый запуск;
- позже взять `Voltage Sensor Module DC 0-25V`;
- перепаять верхний резистор `303` на `104`, чтобы 42V не убили ESP32;
- выход `S` модуля вести на `TTGO GPIO35`;
- потом это пойдёт в web UI и ELRS telemetry.

Caddx Vista:

- отдельная FPV-система;
- не питать от ESP32;
- нужна нормальная линия питания и обдув;
- полезна как глаз для ручного RC-режима.

Mini PC Gigabyte GB-BACE-3160:

- норм как бортовой Linux-наблюдатель;
- OpenCV, USB-вебка, запись, эксперименты;
- не должен напрямую крутить моторы;
- real-time safety остаётся на ESP32.

Arduino:

- на MVP не нужна;
- может пригодиться позже как тупой IO/safety-модуль для датчиков.
