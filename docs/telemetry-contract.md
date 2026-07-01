# Косарь — telemetry contract

Source of truth для будущего server dashboard.

## Принцип

Публичный dashboard read-only. Сервер может показывать telemetry, историю и replay. Управление моторами остаётся локальным: ESP32 принимает команды только через локальные safety-gates.

## MVP payload

```json
{
  "device_id": "kosar-ttgo-01",
  "ts": 1782880000000,
  "link": {
    "crsf": true,
    "hover": true,
    "age_ms": 18
  },
  "battery": {
    "esp_v": 38.4,
    "hover_raw": 384
  },
  "control": {
    "source": "rc",
    "armed": false,
    "profile": "low",
    "speed": 0,
    "steer": 0,
    "left": 0,
    "right": 0
  },
  "hover": {
    "rpm_l": 0,
    "rpm_r": 0,
    "temp": 32,
    "tx": 0,
    "rx": 0,
    "crc_fail": 0
  },
  "events": [
    {
      "seq": 1,
      "ms": 1234,
      "level": "info",
      "type": "boot",
      "msg": "Kosar RC Bench boot"
    }
  ]
}
```

## Не делаем

- Не прокидываем `/api/webdrive` и `/api/motortest` наружу.
- Не делаем публичный joystick.
- Не считаем query token полноценной security-моделью.
- Не подключаем нож к web-control.
