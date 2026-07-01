# [2026-07-01 19:25] Rewrite landing story and build copy

Файл: `agent_docs/development-history/2026-07-01-1925-rewrite-landing-story-build.md`

## Что сделано

- Переписан story-блок лендинга: добавлен смысл проекта, а не generic RC-copy.
- Build-блок заменён с “стенд, корпус, улица” на список того, что реально нужно для робота-газонокосилки.
- Artifact-блок превращён в concept-блок без повторной картинки и без фразы “не учебный чертёж”.
- `docs/landing-design-code.md` обновлён правилами против нейро-слопа.

## Зачем

Сайт начал звучать как сгенерированная презентация: слишком гладко, мало проекта, мало истории. Нужно вернуть голос Косаря: доноры, барахолка, ESP32, ELRS, ржавый уют и инженерный смысл.

## Обновлено

- [x] `apps/kosar-dashboard/public/index.html`
- [x] `docs/landing-design-code.md`
- [x] Документация
- [ ] Тесты: достаточно HTML/CSS smoke после деплоя.

## Связанные решения

- `agent_docs/audits/2026-07-01-1822-design-system-compliance.md`

## Следующие шаги

- При следующем проходе добавить более явный визуальный блок “доноры проекта”: Greenworks, hoverboard, ESP32, ELRS, ST-Link.
