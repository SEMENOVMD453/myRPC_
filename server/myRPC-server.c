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

#define BUFFER_CAP 4096

int verify_user(const char *username) {
    FILE *fp = fopen("/etc/myRPC/users.conf", "r");
    if (!fp) return 0;
    char line[128];
    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\n")] = 0;
        if (strcmp(line, username) == 0) {
            fclose(fp);
            return 1;
        }
    }
    fclose(fp);
    return 0;
}

char *trim_spaces(char *input) {
    while (*input == ' ' || *input == '\t') input++;
    char *end = input + strlen(input) - 1;
    while (end > input && (*end == ' ' || *end == '\t')) *end-- = 0;
    return input;
}

char *shell_escape(const char *src) {
    size_t len = strlen(src);
    char *escaped = malloc(len * 4 + 3);
    if (!escaped) return NULL;
    char *ptr = escaped;
    *ptr++ = ''';
    for (size_t i = 0; i < len; ++i) {
        if (src[i] == '\'') {
            memcpy(ptr, "'\\\''", 4);
            ptr += 4;
        } else {
            *ptr++ = src[i];
        }
    }
    *ptr++ = ''';
    *ptr = 0;
    return escaped;
}

char *read_file(const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) return strdup("(no data)");
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *content = malloc(size + 1);
    if (!content) return strdup("(mem fail)");
    fread(content, 1, size, f);
    content[size] = 0;
    fclose(f);
    return content;
}

void handle_request(const char *json_req, char *response_out) {
    struct json_object *input = json_tokener_parse(json_req);
    struct json_object *output = json_object_new_object();

    if (!input) {
        json_object_object_add(output, "code", json_object_new_int(1));
        json_object_object_add(output, "result", json_object_new_string("Bad JSON"));
        strcpy(response_out, json_object_to_json_string(output));
        json_object_put(output);
        return;
    }

    const char *login = json_object_get_string(json_object_object_get(input, "login"));
    const char *cmd = json_object_get_string(json_object_object_get(input, "command"));

    if (!login || !cmd) {
        json_object_object_add(output, "code", json_object_new_int(1));
        json_object_object_add(output, "result", json_object_new_string("Invalid fields"));
    } else if (!verify_user(login)) {
        json_object_object_add(output, "code", json_object_new_int(1));
        json_object_object_add(output, "result", json_object_new_string("Access denied"));
    } else {
        char tempfile[] = "/tmp/mrpc_XXXXXX";
        int tmpfd = mkstemp(tempfile);
        if (tmpfd < 0) {
            log_error("tmpfile: %s", strerror(errno));
            json_object_object_add(output, "code", json_object_new_int(1));
            json_object_object_add(output, "result", json_object_new_string("Tmp fail"));
        } else {
            close(tmpfd);
            char fout[256], ferr[256];
            snprintf(fout, sizeof(fout), "%s.out", tempfile);
            snprintf(ferr, sizeof(ferr), "%s.err", tempfile);

            char *safe = shell_escape(cmd);
            if (!safe) {
                json_object_object_add(output, "code", json_object_new_int(1));
                json_object_object_add(output, "result", json_object_new_string("Mem error"));
            } else {
                char exec_line[1024];
                snprintf(exec_line, sizeof(exec_line), "sh -c %s > %s 2> %s", safe, fout, ferr);
                free(safe);
                int ret = system(exec_line);
                char *msg = read_file(ret == 0 ? fout : ferr);
                json_object_object_add(output, "code", json_object_new_int(ret == 0 ? 0 : 1));
                json_object_object_add(output, "result", json_object_new_string(msg));
                free(msg);
            }
        }
    }

    strcpy(response_out, json_object_to_json_string(output));
    json_object_put(input);
    json_object_put(output);
}

int main() {
    log_info("Daemon Init");

    int port_num = 1234;
    int tcp = 1;

    FILE *config = fopen("/etc/myRPC/myRPC.conf", "r");
    if (config) {
        char buf[256];
        while (fgets(buf, sizeof(buf), config)) {
            buf[strcspn(buf, "\n")] = 0;
            char *line = trim_spaces(buf);
            if (line[0] == '#' || strlen(line) == 0) continue;
            if (strstr(line, "port")) sscanf(line, "port = %d", &port_num);
            else if (strstr(line, "socket_type")) {
                char proto[16];
                if (sscanf(line, "socket_type = %15s", proto) == 1) tcp = strcmp(proto, "dgram") != 0;
            }
        }
        fclose(config);
    } else {
        log_warning("Config missing: %s", strerror(errno));
    }

    int srv_sock = socket(AF_INET, tcp ? SOCK_STREAM : SOCK_DGRAM, 0);
    if (srv_sock < 0) {
        log_error("Socket fail: %s", strerror(errno));
        exit(1);
    }

    struct sockaddr_in srv_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(port_num),
        .sin_addr.s_addr = INADDR_ANY
    };

    if (bind(srv_sock, (struct sockaddr *)&srv_addr, sizeof(srv_addr)) < 0) {
        log_error("Bind fail: %s", strerror(errno));
        exit(1);
    }

    if (tcp && listen(srv_sock, 5) < 0) {
        log_error("Listen fail: %s", strerror(errno));
        exit(1);
    }

    log_info("Daemon ready at %d/%s", port_num, tcp ? "TCP" : "UDP");

    while (1) {
        struct sockaddr_in cli_addr;
        socklen_t cli_len = sizeof(cli_addr);
        char buffer[BUFFER_CAP] = {0};

        if (tcp) {
            int cli_sock = accept(srv_sock, (struct sockaddr *)&cli_addr, &cli_len);
            if (cli_sock < 0) {
                log_error("Accept fail: %s", strerror(errno));
                continue;
            }
            ssize_t rcv = recv(cli_sock, buffer, sizeof(buffer) - 1, 0);
            if (rcv <= 0) {
                log_error("TCP recv fail: %s", strerror(errno));
                close(cli_sock);
                continue;
            }
            buffer[rcv] = 0;
            log_info("TCP: %s", buffer);
            char reply[BUFFER_CAP];
            handle_request(buffer, reply);
            send(cli_sock, reply, strlen(reply), 0);
            close(cli_sock);
        } else {
            ssize_t rcv = recvfrom(srv_sock, buffer, sizeof(buffer) - 1, 0,
                                   (struct sockaddr *)&cli_addr, &cli_len);
            if (rcv < 0) {
                log_error("UDP recvfrom fail: %s", strerror(errno));
                continue;
            }
            buffer[rcv] = 0;
            log_info("UDP: %s", buffer);
            char reply[BUFFER_CAP];
            handle_request(buffer, reply);
            sendto(srv_sock, reply, strlen(reply), 0, (struct sockaddr *)&cli_addr, cli_len);
        }
    }

    close(srv_sock);
    return 0;
}
