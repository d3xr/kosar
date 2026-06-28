# EFeru Firmware Notes

Основной кандидат: https://github.com/EFeru/hoverboard-firmware-hack-FOC

Проверено 2026-06-28:

- GitHub repo активен, `main`, последний push через `gh`: 2026-01-14.
- README прямо указывает поддержку `STM32F103RCT6` и `GD32F103RCT6`.
- Доступные варианты управления: `VARIANT_USART`, `VARIANT_PPM`, `VARIANT_PWM`, `VARIANT_IBUS`.
- Build workflow в GitHub Actions активен.

## Важные файлы upstream

- `platformio.ini` - выбор env: `VARIANT_PPM`, `VARIANT_PWM`, `VARIANT_IBUS`, `VARIANT_USART`.
- `Inc/config.h` - батарея, лимиты, режим управления, входы, deadband.
- `Makefile` - цели `flash` и `unlock`.
- `Arduino/hoverserial/hoverserial.ino` - пример serial-пакета для `VARIANT_USART`.

## Sideboard cables

EFeru README описывает два 4-pin sideboard cable:

- USART2 / left sensor cable: не считать 5V tolerant.
- USART3 / right sensor cable: 5V tolerant по README и рекомендуем для Arduino/RC входов.

Практическое правило: для первого RC входа использовать правый cable, но перед подключением все равно прозвонить GND и питание мультиметром.

## PPM

В `Inc/config.h` для `VARIANT_PPM` по умолчанию:

- `CONTROL_PPM_RIGHT`.
- Channel 1: steering.
- Channel 2: speed.
- `PPM_NUM_CHANNELS 6`.
- `PRI_INPUT1` и `PRI_INPUT2` от `-1000` до `1000` с deadband.

Подходит только если конкретный приемник умеет PPM Sum. Типичный ELRS serial receiver сам по себе PPM не дает.

## PWM

В `VARIANT_PWM`:

- правый sensor cable;
- CH1 steering;
- CH2 speed;
- отдельные PWM-сигналы от приемника.

Это самый короткий путь, если под рукой есть ELRS PWM receiver или PWM-конвертер.

## iBUS

`VARIANT_IBUS` - FlySky iBUS на USART3 115200. Это не ELRS CRSF. Использовать только с приемником, который реально выдает iBUS.

## USART

`VARIANT_USART` принимает serial-пакеты `start / steer / speed / checksum`. Это лучший путь для ESP32 bridge:

```c
Command.start = 0xABCD;
Command.steer = steer;
Command.speed = speed;
Command.checksum = Command.start ^ Command.steer ^ Command.speed;
```

Начальный target: USART3/right cable, 115200 baud. EFeru serial timeout оставляем вторым слоем failsafe: ESP32 должен слать `0,0` при потере CRSF, а при своей аварии переставать слать пакеты, чтобы прошивка ушла в timeout-stop.

## Начальные настройки для MVP

Рекомендованный baseline перед bench-тестом:

```c
#define BAT_CELLS      10
#define CTRL_TYP_SEL   FOC_CTRL
#define CTRL_MOD_REQ   VLT_MODE
#define FIELD_WEAK_ENA 0
#define N_MOT_MAX      60
```

Лимиты `I_MOT_MAX`, `I_DC_MAX`, `N_MOT_MAX`, `DEFAULT_RATE`, `SPEED_COEFFICIENT`, `STEER_COEFFICIENT` подбирать после вывешенного теста. Для первой земли целиться в медленную тележку, не в максимальную скорость.

`TANK_STEERING` на MVP не включать: EFeru штатно микширует `steer + speed` в левый/правый мотор, а прямое управление каждым колесом усложнит RC failsafe и калибровку.

## Риски

- `unlock` стирает штатную прошивку.
- Ошибка пинов sideboard cable может сжечь приемник/ESP32.
- Дефолтные скоростные лимиты слишком агрессивны для 10" косилочной платформы.
- PPM чувствителен к шуму; провода короткие, желательно экранирование/ферриты.
