# Конфиги

Они нужны для работы сервера.

## myRPC-server.service

Файл для активации демона

Поместите его в:
/lib/systemd/system/myRPC-server.service

Скопируйте собранный сервер сюда:

   ```bash
   cp -f myRPC/bin/myRPC-server /usr/local/bin/myRPC-server
   ```

## Конфиги

Обязательно имейте конфиги по пути:
/etc/myRPC/users.conf
/etc/myRPC/myRPC.conf
Настройка на главной странице! 
