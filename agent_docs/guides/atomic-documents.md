# Атомарные документы

Одна итерация, решение, аудит или расследование — один файл.

## Где применять

- `agent_docs/development-history/`
- `agent_docs/adr/`
- будущие `agent_docs/investigations/`

## Где не применять

- `AGENTS.md`
- `README.md`
- `agent_docs/index.md`
- `agent_docs/architecture.md`
- `agent_docs/glossary.md`
- `docs/*.md`

Эти документы описывают актуальное состояние.

## Имена файлов

```text
YYYY-MM-DD-HHMM-short-title.md
```

Правила:

- время локальное для проекта/сессии;
- `short-title` латиницей в kebab-case;
- не использовать ручную сквозную нумерацию;
- README коллекции не обновлять при каждой новой записи.
