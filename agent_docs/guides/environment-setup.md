# Настройка окружения проекта

## Правила

- `.env` виден пользователю в IDE, но игнорируется git и AI-агентами.
- `firmware/esp32-crsf-web/src/wifi_local.h` хранит локальные Wi-Fi credentials и не коммитится.
- `firmware/upstream/` не коммитится: это checkout upstream-прошивки.
- `.env.example` содержит только безопасные публичные значения.

## Проверочные файлы

- `.gitignore` — игнорирует `.env`, `.env.*`, `wifi_local.h`, `firmware/upstream/`, `.pio/`, build artifacts.
- `.cursorignore` — скрывает `.env` и `wifi_local.h`.
- `.vscode/settings.json` — не прячет `.env` от пользователя в explorer.
- `.editorconfig` — фиксирует LF, UTF-8, final newline.
