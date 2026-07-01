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

## Хостинг

Папка `public/` самодостаточная: HTML, CSS, JS, изображения, `CNAME`.

Для nginx:

```nginx
server {
    server_name kosar.vyroslo.ru;
    root /var/www/kosar-dashboard;
    index index.html;

    location / {
        try_files $uri $uri/ /index.html;
    }
}
```

Командный контур моторов в этот лендинг не выносить. Публичная версия только read-only: история, mock/replay, telemetry позже.
