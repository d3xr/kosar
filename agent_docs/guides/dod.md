# Критерии завершенности

## Минимальный DoD

- [ ] Результат соответствует запросу.
- [ ] Изменения не выходят за рамки задачи.
- [ ] Документация обновлена, если менялись структура, протоколы, сайт или железный workflow.
- [ ] Для нетривиальной итерации добавлена запись в `agent_docs/development-history/`.
- [ ] Для значимого решения добавлена запись в `agent_docs/adr/`.
- [ ] Запущены релевантные проверки или явно указано, почему они не запускались.

## Для сайта

- [ ] Локально открывается `apps/kosar-dashboard/public/index.html`.
- [ ] Ссылки на GitHub, `Шкаф`, assets, manifest, sitemap и `llms.txt` актуальны.
- [ ] Public surface остаётся read-only.
- [ ] Copy соответствует `docs/landing-design-code.md`.

## Для firmware

- [ ] `pio run` проходит до upload.
- [ ] Пины, UART baudrate и board assumptions явно указаны.
- [ ] Hardware-facing изменения описывают bench check и rollback.

## Для docs

- [ ] Документ связан с `agent_docs/index.md` или существующим hub-файлом.
- [ ] Нет приватных абсолютных путей, credentials и содержимого ignored-файлов.
