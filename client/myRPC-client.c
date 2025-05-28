#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <json-c/json.h>
#include <getopt.h>
#include <pwd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

#define BUF_SIZE 4096  // Размер буфера для приёма данных

// Функция вывода справки
void print_usage(const char *prog) {
    printf("Usage: %s [OPTIONS]\n", prog);
    printf("Options:\n");
    printf("  -c, --command \"bash_command\"    Bash command to execute\n");
    printf("  -h, --host \"ip_addr\"             Server IP address\n");
    printf("  -p, --port PORT                  Server port\n");
    printf("  -s, --stream                     Use stream (TCP) socket\n");
    printf("  -d, --dgram                      Use datagram (UDP) socket\n");
    printf("      --help                       Show this help message\n");
}

int main(int argc, char *argv[]) {
    char *command = NULL, *host = NULL;
    int port = 0, use_stream = 0, use_dgram = 0;

    // Определение поддерживаемых опций командной строки
    static struct option long_options[] = {
        {"command", required_argument, 0, 'c'},
        {"host", required_argument, 0, 'h'},
        {"port", required_argument, 0, 'p'},
        {"stream", no_argument, 0, 's'},
        {"dgram", no_argument, 0, 'd'},
        {"help", no_argument, 0, 0},
        {0, 0, 0, 0}
    };

    // Разбор аргументов командной строки
    int opt, option_index = 0;
    while ((opt = getopt_long(argc, argv, "c:h:p:sd", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'c': command = strdup(optarg); break; // Команда для отправки на сервер
            case 'h': host = strdup(optarg); break;    // IP адрес сервера
            case 'p': port = atoi(optarg); break;      // Порт сервера
            case 's': use_stream = 1; break;           // TCP
            case 'd': use_dgram = 1; break;            // UDP
            case 0:   // Обработка --help
                if (strcmp(long_options[option_index].name, "help") == 0) {
                    print_usage(argv[0]);
                    exit(0);
                }
                break;
            default:
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    // Проверка обязательных параметров
    if (!command || !host || !port || (!use_stream && !use_dgram)) {
        print_usage(argv[0]);
        exit(EXIT_FAILURE);
    }

    // Получение имени текущего пользователя
    struct passwd *pw = getpwuid(getuid());
    if (!pw) {
        perror("getpwuid");
        exit(EXIT_FAILURE);
    }
    const char *username = pw->pw_name;

    // Формирование JSON-запроса
    struct json_object *req = json_object_new_object();
    json_object_object_add(req, "login", json_object_new_string(username));
    json_object_object_add(req, "command", json_object_new_string(command));
    const char *json_str = json_object_to_json_string(req);

    // Создание сокета (TCP или UDP)
    int sockfd;
    struct sockaddr_in servaddr;
    sockfd = socket(AF_INET, (use_stream ? SOCK_STREAM : SOCK_DGRAM), 0);
    if (sockfd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Настройка адреса сервера
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    inet_pton(AF_INET, host, &servaddr.sin_addr);

    if (use_stream) {
        // TCP: установка соединения
        if (connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
            perror("connect");
            close(sockfd);
            exit(EXIT_FAILURE);
        }

        // Отправка запроса
        send(sockfd, json_str, strlen(json_str), 0);

        // Приём ответа
        char buf[BUF_SIZE] = {0};
        ssize_t n = recv(sockfd, buf, sizeof(buf) - 1, 0);
        if (n > 0) {
            buf[n] = '\0';
            printf("Ответ сервера: %s\n", buf);
        } else {
            perror("recv");
        }

    } else {
        // UDP: установка таймаута на приём
        struct timeval tv = {5, 0};
        setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        // Отправка запроса
        sendto(sockfd, json_str, strlen(json_str), 0, (struct sockaddr*)&servaddr, sizeof(servaddr));

        // Приём ответа
        char buf[BUF_SIZE] = {0};
        socklen_t len = sizeof(servaddr);
        ssize_t n = recvfrom(sockfd, buf, sizeof(buf) - 1, 0, (struct sockaddr*)&servaddr, &len);
        if (n > 0) {
            buf[n] = '\0';
            printf("Ответ сервера: %s\n", buf);
        } else {
            // Обработка таймаута
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                fprintf(stderr, "Timeout waiting for UDP response\n");
            } else {
                perror("recvfrom");
            }
        }
    }

    // Очистка ресурсов
    close(sockfd);
    free(command);
    free(host);
    json_object_put(req);

    return 0;
}
