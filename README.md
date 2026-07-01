# Косарь

Гаражный RC-проект: берём Greenworks, донорский 10" гироскутер, RadioMaster Boxer ELRS и делаем странную, полезную, потенциально опасную тележку, которая сначала просто едет.

Дух проекта: **строим из того, что есть, но не тупим там, где можно спалить плату или пальцы**.

## Кто я в этой вселенной

Я тут не завлаб и не Scrum-мастер. Я — твой бортовой инженер из мастерской: держу карту проводов, помню команды, проверяю “это сейчас не бахнет?”, а ты крутишь железо и принимаешь решения.

Моя работа:

- убирать туман;
- давать короткие следующие шаги;
- не превращать фановую сборку в диплом;
- останавливать только там, где реально можно сжечь железо или сделать опасную штуку.

## Текущий квест

Гироскутерная плата прошита через ST-Link. ESP/Web UI/ELRS уже крутят моторы на стенде. Сейчас задача не “собрать робота-косилку”. Сейчас задача: **собрать жёсткий временный drive module без ножа и проверить первую езду на низком режиме**.

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

Патчи с безопасным конфигом:

```bash
firmware/patches/stage1-safe-usart3-config.patch
```

## Главный документ

Вся практическая инструкция теперь одна:

[docs/garage-runbook.md](docs/garage-runbook.md)

Механический MVP:

[docs/mechanical-mvp-drive-module.md](docs/mechanical-mvp-drive-module.md)

## Публичный лендинг и dashboard

Статический Rustpunk Cozy лендинг лежит тут:

[apps/kosar-dashboard](apps/kosar-dashboard)

Он рассчитан на `kosar.vyroslo.ru` на твоём Timeweb-хостинге и пока работает как read-only витрина с mock telemetry. Управление моторами наружу не выносится.

Telemetry contract:

[docs/telemetry-contract.md](docs/telemetry-contract.md)

## Короткое правило

Если непонятно, куда паять, не паять. Сначала фото платы крупно, потом разметка, потом паяльник.
