# [2026-07-02 12:42] Recompress landing images

Файл: `agent_docs/development-history/2026-07-02-1242-recompress-landing-images.md`

## Что сделано

- Основные WebP-ассеты лендинга пережаты из PNG-исходников через `cwebp -q 96 -m 6 -af`.
- `docs/design-system.md` получил policy по экспорту изображений.

## Зачем

Старые WebP были слишком маленькие для полноэкранных AI-рендеров: hero 217 KB при 1672×941, build-story 271 KB. На таком весе видны мыло, бэндинг и потеря фактуры.

## Обновлено

- [x] `apps/kosar-dashboard/public/assets/*.webp`
- [x] `docs/design-system.md`
- [ ] Визуальный screenshot не снимался; проверены размеры ассетов и HTTP delivery.

## Следующие шаги

- Для следующего hero перегенерировать или апскейлить исходник до 2400-3000 px шириной.
