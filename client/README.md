# myRPC-client

---

`myRPC-client` — это консольная утилита, предназначенная для отправки команд серверу `myRPC-server`.

##  Возможности

- Отправка команд на сервер
- Выбор типа сокета: TCP или UDP
- Чтение конфигурации с помощью аргументов CLI

##  Использование

```bash
myRPC-client -c "<команда>" -h <ip> -p <порт> [-d|-s]
```

Примеры:
```bash
myRPC-client -c "echo Hello" -h 127.0.0.1 -p 25565 -s
```

## Аргументы

- `-c` — команда
- `-h` — IP-адрес сервера
- `-p` — порт
- `-s` — TCP
- `-d` — UDP

##  Лицензия

MIT
