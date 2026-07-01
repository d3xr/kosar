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

Текущий live:

```text
https://kosar.vyroslo.ru
```

Текущий серверный контур:

```text
DNS owner: self-hosted ns1.vyroslo.ru / ns2.vyroslo.ru
A record: kosar.vyroslo.ru -> 77.95.206.196
Web root: /var/www/kosar
Nginx vhost: /etc/nginx/sites-available/kosar
TLS: Let's Encrypt / certbot nginx
```

Для ручного деплоя:

1. Загрузить содержимое `apps/kosar-dashboard/public/` в web root.
2. Проверить, что открывается `/index.html`, `/src/styles.css`, `/assets/kosar-hero-rustpunk.webp`.
3. Проверить HTTP -> HTTPS redirect.
4. Проверить, что страница содержит ссылку на `https://github.com/d3xr/kosar`.

SEO/social/GEO минимум:

```text
/favicon.svg
/favicon.ico
/apple-touch-icon.png
/icon-192.png
/icon-512.png
/site.webmanifest
/robots.txt
/sitemap.xml
/llms.txt
/assets/kosar-og-image.png
/assets/kosar-mvp-drive-art.webp
```

В `index.html` должны быть canonical, OG, Twitter Card и JSON-LD. Не добавлять metadata для API/MCP/OAuth/commerce, пока таких публичных интерфейсов реально нет.

Для nginx:

```nginx
server {
    server_name kosar.vyroslo.ru;
    root /var/www/kosar;
    index index.html;

    location = /mvp-greenworks-hover-drive.svg {
        return 410;
    }

    location = /site.webmanifest {
        types { application/manifest+json webmanifest; }
        try_files $uri =404;
    }

    location / {
        try_files $uri $uri/ /index.html;
    }
}
```

Командный контур моторов в этот лендинг не выносить. Публичная версия только read-only: история, mock/replay, telemetry позже.
