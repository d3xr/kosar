# GitHub Research

Проверено 2026-06-28 через `gh repo view` и локальные shallow-клоны.

## Core

| Repo | Роль | Состояние |
|---|---|---|
| https://github.com/EFeru/hoverboard-firmware-hack-FOC | Основная прошивка для Gen1 hoverboard mainboard | Активен; `main`; push 2026-01-14; README подтверждает `GD32F103RCT6`, PPM/PWM/iBUS/UART |
| https://github.com/RoboDurden/Hoverboard-Firmware-Hack-Gen2.x | Запасной путь для Gen2/split boards | Активен; push 2026-04-30; нужен только если вскрытие покажет не Gen1 |
| https://github.com/ExpressLRS/ExpressLRS | Радиолинк ELRS / CRSF | Активен; push 2026-06-27 |
| https://github.com/AlfredoSystems/AlfredoCRSF | Arduino/ESP32 CRSF parsing | Активен; push 2026-02-11; кандидат для ESP32 bridge |

## RC / hoverboard references

| Repo | Роль | Ограничение |
|---|---|---|
| https://github.com/DaveZG/esp32-powerwheels | ESP32 управляет hoverboard controller по UART | Старый, push 2022-05-26; брать как референс |
| https://github.com/jebc/hoverboard-firmware-hack-FOC-pwmLR | RC/PWM fork EFeru | Очень маленький/старый fork; не брать как основу |
| https://github.com/gaucho1978/CHEAP-LAWNMOWER-ROBOT-FROM-HOVERBOARD | Газонокосилка на hoverboard базе | Старый, push 2021-05-24; полезен по механике и общей идее |

## Future autonomy

| Repo | Роль | Комментарий |
|---|---|---|
| https://github.com/ClemensElflein/OpenMower | RTK/ROS робот-косилка на базе SUMEC/YardForce | Активен; push 2026-06-24; не для Stage 1 |
| https://github.com/Ardumower/Sunray | Альтернативная автономная косилка | Активен; push 2026-06-26; не для Stage 1 |
| https://github.com/hoverboard-robotics/hoverboard-driver | ROS driver для UART-controlled hoverboard | Активен; push 2026-02-11; будущий ROS reference |

## Решение по зависимостям

- В этот repo пока не копировать upstream firmware.
- При необходимости использовать `firmware/upstream/`, который игнорируется git.
- Перед публикацией на GitHub добавить license review, если появится чужой код, схемы или картинки.
