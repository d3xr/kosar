# Kosar dashboard landing

Статический лендинг и read-only demo dashboard проекта «Косарь».

## Локальный запуск

```bash
cd /Users/d3xr/Documents/Косарь/apps/kosar-dashboard/public
python3 -m http.server 8787
```

Открыть:

```text
http://127.0.0.1:8787/
```

## Хостинг на Timeweb / любом обычном веб-хостинге

Папка `public/` самодостаточная: HTML, CSS, JS, изображения.

Для Timeweb:

1. Создать поддомен `kosar.vyroslo.ru`.
2. Назначить ему отдельную папку на хостинге.
3. Загрузить содержимое `apps/kosar-dashboard/public/` в эту папку.
4. Проверить, что открывается `/index.html`, `/src/styles.css`, `/assets/kosar-hero-rustpunk.webp`.

Для nginx:

```nginx
server {
    server_name kosar.vyroslo.ru;
    root /var/www/kosar-dashboard/public;
    index index.html;

    location / {
        try_files $uri $uri/ /index.html;
    }
}
```

Командный контур моторов в этот лендинг не выносить. Публичная версия только read-only: история, mock/replay, telemetry позже.
