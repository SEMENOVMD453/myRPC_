#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/types.h>
#include <json-c/json.h>
#include <signal.h>
#include <sys/stat.h>
#include "mysyslog.h"

#define BUF_LEN 4096

// Проверка логина в списке пользователей
int is_user_valid(const char *name) {
    FILE *file = fopen("/etc/myRPC/users.conf", "r");
    if (!file) return 0;
    char row[128];
    while (fgets(row, sizeof(row), file)) {
        row[strcspn(row, "\n")] = 0;
        if (strcmp(row, name) == 0) {
            fclose(file);
            return 1;
        }
    }
    fclose(file);
    return 0;
}

// Удаление пробелов и табов
char *clean_spaces(char *txt) {
    while (*txt == ' ' || *txt == '\t') txt++;
    char *tail = txt + strlen(txt) - 1;
    while (tail > txt && (*tail == ' ' || *tail == '\t')) *tail-- = 0;
    return txt;
}

// Экранирование для оболочки
char *escape_cmd(const char *str) {
    size_t l = strlen(str);
    char *res = malloc(l * 4 + 3);
    if (!res) return NULL;
    char *p = res;
    *p++ = '\'';
    for (size_t i = 0; i < l; ++i) {
        if (str[i] == '\'') {
            memcpy(p, "'\\''", 4);
            p += 4;
        } else {
            *p++ = str[i];
        }
    }
    *p++ = '\'';
    *p = 0;
    return res;
}

// Чтение всего файла в строку
char *load_file(const char *fname) {
    FILE *f = fopen(fname, "r");
    if (!f) return strdup("(no data)");
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(sz + 1);
    if (!buf) return strdup("(mem fail)");
    fread(buf, 1, sz, f);
    buf[sz] = 0;
    fclose(f);
    return buf;
}

// Обработка JSON-запроса
void process_json(const char *req, char *res_buf) {
    struct json_object *root = json_tokener_parse(req);
    struct json_object *reply = json_object_new_object();

    if (!root) {
        json_object_object_add(reply, "code", json_object_new_int(1));
        json_object_object_add(reply, "result", json_object_new_string("Bad JSON"));
        strcpy(res_buf, json_object_to_json_string(reply));
        json_object_put(reply);
        return;
    }

    const char *user = json_object_get_string(json_object_object_get(root, "login"));
    const char *action = json_object_get_string(json_object_object_get(root, "command"));

    if (!user || !action) {
        json_object_object_add(reply, "code", json_object_new_int(1));
        json_object_object_add(reply, "result", json_object_new_string("Invalid fields"));
    } else if (!is_user_valid(user)) {
        json_object_object_add(reply, "code", json_object_new_int(1));
        json_object_object_add(reply, "result", json_object_new_string("Access denied"));
    } else {
        char temp_path[] = "/tmp/rpc_tmp_XXXXXX";
        int fd = mkstemp(temp_path);
        if (fd < 0) {
            log_error("mkstemp error: %s", strerror(errno));
            json_object_object_add(reply, "code", json_object_new_int(1));
            json_object_object_add(reply, "result", json_object_new_string("Temp file fail"));
        } else {
            close(fd);
            char out_f[256], err_f[256];
            snprintf(out_f, sizeof(out_f), "%s.out", temp_path);
            snprintf(err_f, sizeof(err_f), "%s.err", temp_path);

            char *esc = escape_cmd(action);
            if (!esc) {
                json_object_object_add(reply, "code", json_object_new_int(1));
                json_object_object_add(reply, "result", json_object_new_string("Mem alloc error"));
            } else {
                char cmd_line[1024];
                snprintf(cmd_line, sizeof(cmd_line), "sh -c %s > %s 2> %s", esc, out_f, err_f);
                free(esc);
                int code = system(cmd_line);
                char *out = load_file(code == 0 ? out_f : err_f);
                json_object_object_add(reply, "code", json_object_new_int(code == 0 ? 0 : 1));
                json_object_object_add(reply, "result", json_object_new_string(out));
                free(out);
            }
        }
    }

    strcpy(res_buf, json_object_to_json_string(reply));
    json_object_put(root);
    json_object_put(reply);
}

int main() {
    log_info("Start of RPC Daemon");

    int listen_port = 1234;
    int tcp_mode = 1;

    FILE *cfg = fopen("/etc/myRPC/myRPC.conf", "r");
    if (cfg) {
        char conf_line[256];
        while (fgets(conf_line, sizeof(conf_line), cfg)) {
            conf_line[strcspn(conf_line, "\n")] = 0;
            char *opt = clean_spaces(conf_line);
            if (opt[0] == '#' || strlen(opt) == 0) continue;

            if (strstr(opt, "port")) sscanf(opt, "port = %d", &listen_port);
            else if (strstr(opt, "socket_type")) {
                char proto[16];
                if (sscanf(opt, "socket_type = %15s", proto) == 1) {
                    tcp_mode = strcmp(proto, "dgram") != 0;
                }
            }
        }
        fclose(cfg);
    } else {
        log_warning("Failed to open config: %s", strerror(errno));
    }

    int sock = socket(AF_INET, tcp_mode ? SOCK_STREAM : SOCK_DGRAM, 0);
    if (sock < 0) {
        log_error("Socket creation failed: %s", strerror(errno));
        exit(1);
    }

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(listen_port),
        .sin_addr.s_addr = INADDR_ANY
    };

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        log_error("Bind failed: %s", strerror(errno));
        exit(1);
    }

    if (tcp_mode && listen(sock, 5) < 0) {
        log_error("Listen failed: %s", strerror(errno));
        exit(1);
    }

    log_info("RPC Ready on port %d (%s)", listen_port, tcp_mode ? "TCP" : "UDP");

    while (1) {
        struct sockaddr_in client;
        socklen_t cl_len = sizeof(client);
        char in_buf[BUF_LEN] = {0};

        if (tcp_mode) {
            int client_sock = accept(sock, (struct sockaddr *)&client, &cl_len);
            if (client_sock < 0) {
                log_error("Accept failed: %s", strerror(errno));
                continue;
            }

            ssize_t rcv = recv(client_sock, in_buf, sizeof(in_buf) - 1, 0);
            if (rcv <= 0) {
                log_error("Receive failed: %s", strerror(errno));
                close(client_sock);
                continue;
            }

            in_buf[rcv] = 0;
            log_info("TCP request: %s", in_buf);

            char reply[BUF_LEN];
            process_json(in_buf, reply);
            send(client_sock, reply, strlen(reply), 0);
            close(client_sock);

        } else {
            ssize_t rcv = recvfrom(sock, in_buf, sizeof(in_buf) - 1, 0,
                                   (struct sockaddr *)&client, &cl_len);
            if (rcv < 0) {
                log_error("Recvfrom failed: %s", strerror(errno));
                continue;
            }

            in_buf[rcv] = 0;
            log_info("UDP request: %s", in_buf);

            char reply[BUF_LEN];
            process_json(in_buf, reply);
            sendto(sock, reply, strlen(reply), 0, (struct sockaddr *)&client, cl_len);
        }
    }

    close(sock);
    return 0;
}

