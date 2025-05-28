# myRPC-server

---

`myRPC-server` — это демон на основе сокетов, принимающий команды от клиентов и проверяющий их авторизацию.

##  Возможности

- Поддержка TCP (stream) и UDP (dgram) сокетов
- Чтение конфигурации из `/etc/myRPC/myRPC.conf`
- Проверка пользователей по `/etc/myRPC/users.conf`
- Логирование через библиотеку `smylog`

##  Конфигурация

Файл `/etc/myRPC/myRPC.conf`:
```ini
port = 25565
socket_type = stream
```

Файл `/etc/myRPC/users.conf`:
```
alice
bob
```

##  Запуск

Служба systemd:
```bash
systemctl start myRPC-server
systemctl status myRPC-server
systemctl restart myRPC-server
```

##  Ответ сервера

- В случае успеха — выполняется команда и возвращается результат.
- В случае ошибки — отправляется JSON-ошибка.

##  Лицензия

MIT
