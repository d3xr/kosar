# Логирование

Использовать для scripts/deploy/integration задач, где есть сетевые запросы, SSH, GitHub, DNS, VPS или внешние API.

## Правила

- Один запуск — одна папка `logs/run-%timestamp%`.
- Внутри папки основной файл `_log.md`.
- Одна запись = одно событие.
- Указывать время и уровень: `INFO`, `WARN`, `ERROR`.
- Не писать секреты, токены, Wi-Fi credentials и приватные ключи.
- Логи старше трёх дней удалять, если нет отдельного решения хранить их дольше.

## Шаблон

```markdown
## События

- [HH:MM:SS] [INFO] start - ok
- [HH:MM:SS] [INFO] request <service> <method> <endpoint> - ok
- [HH:MM:SS] [ERROR] <action> - <error>
- [HH:MM:SS] [INFO] finish - ok
```
