# Глоссарий проекта

## Термины

| Термин | Значение | Контекст |
|--------|----------|----------|
| Косарь | Рабочее название RC-переделки Greenworks | Весь проект |
| Stage 1 | Прошить и проверить RC-ходовую без ножа | Текущий этап |
| Greenworks 48V | Аккумуляторная косилка, будущая механическая база | Нож и штатная батарея отключены на MVP |
| Hoverboard mainboard | Родная плата гироскутера для управления двумя hub motors | EFeru FOC |
| Sideboard cable | 4-pin кабели бывших боковых плат гироскутера | UART/PWM/PPM/iBUS входы, нужна прозвонка |
| E-stop | Физическая аварийная остановка с разрывом питания привода | Обязательный safety gate |

## Аббревиатуры

| Сокращение | Расшифровка |
|------------|-------------|
| ELRS | ExpressLRS |
| CRSF | Crossfire serial protocol |
| PWM | Pulse Width Modulation |
| PPM | Pulse Position Modulation |
| iBUS | FlySky serial RC protocol |
| FOC | Field Oriented Control |
| MCU | Microcontroller Unit |
| SWD | Serial Wire Debug |

## Кодовые названия и нейминги

- **EFeru:** `https://github.com/EFeru/hoverboard-firmware-hack-FOC`
- **ExpressLRS:** `https://github.com/ExpressLRS/ExpressLRS`
- **AlfredoCRSF:** `https://github.com/AlfredoSystems/AlfredoCRSF`
- **OpenMower:** будущий reference для автономии, не Stage 1.
