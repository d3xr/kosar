# Косарь

RC-переделка аккумуляторной Greenworks 48V в управляемую тяговую платформу на мотор-колесах от 10" гироскутера.

## Статус

Дата фиксации: 2026-06-28.

Этап 1: прошить плату гироскутера, добиться предсказуемого дистанционного движения на родной гироскутерной платформе, затем перенести ходовую на Greenworks. Нож Greenworks на MVP физически отключен и не участвует в тестах.

## Архитектура MVP

- Ходовая: два мотор-колеса от 10" гироскутера.
- Контроллер: родная Gen1-плата гироскутера, предположительно `GD32F103RCT6`.
- Прошивка: `EFeru/hoverboard-firmware-hack-FOC`.
- Питание привода: родная 10S батарея гироскутера, около 36V nominal / 42V full.
- Питание ножа Greenworks: отдельная штатная система, на первом этапе отключена.
- Радио: RadioMaster Boxer ELRS; конкретный тракт зависит от приемника.

## Главные решения

- Не подавать Greenworks 48V на плату гироскутера: полный заряд около 54V выше нормального диапазона generic hoverboard-плат.
- Не начинать с автономии, RTK, OpenMower или режущего узла.
- Не вендорить чужие firmware-репозитории в этот repo до лицензионного и архитектурного решения.
- Для прямого RC без ESP32 использовать только тот вход, который реально поддерживает приемник: PWM/PPM/iBUS.
- Для типичного ELRS serial receiver основной путь после проверки железа: `CRSF -> ESP32 -> EFeru UART`.

## Документы

- [Stage 1 RC bring-up](docs/01-stage-1-rc-bringup.md)
- [EFeru firmware notes](docs/02-firmware-eferu.md)
- [RC / ELRS decision](docs/03-rc-elrs-decision.md)
- [Safety gates and test matrix](docs/04-safety-and-test-matrix.md)
- [GitHub research](docs/05-github-research.md)
- [Inventory and open questions](docs/06-inventory-and-open-questions.md)
- [ADR-0001: Stage 1 control path](docs/adr-0001-stage-1-control-path.md)

## Agent Docs

- [AGENTS.md](AGENTS.md) — project-local правила работы агентов.
- [agent_docs/index.md](agent_docs/index.md) — навигация по процессной документации.
- [agent_docs/architecture.md](agent_docs/architecture.md) — текущая архитектурная карта.
- [agent_docs/development-history/](agent_docs/development-history/) — атомарная история итераций.
- [agent_docs/adr/](agent_docs/adr/) — атомарный журнал решений.

## Current Verdicts

| Gate | Verdict | Комментарий |
|---|---:|---|
| Product / Scope | PASS | Этап 1 ограничен ходовой RC-платформой без ножа и автономии. |
| Architect | WARN | Нужно подтвердить Gen1-плату фото и прозвонкой разъемов. |
| Domain / RC | WARN | ELRS CRSF не является PPM/iBUS; нужен конкретный приемник или ESP32 bridge. |
| Firmware | WARN | EFeru подходит, но Gen1/pinout/sideboard polarity еще нужно подтвердить физически. |
| QA / Safety | WARN | Failsafe, e-stop, питание приемника и лимиты еще не доказаны тестом. |
| Release / GitOps | PASS | Пока только локальная документация и первый git commit. |

`UNKNOWN` не считается `PASS`; пока незакрытые gates блокируют тест на земле.
