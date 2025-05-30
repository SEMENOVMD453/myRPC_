#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <json-c/json.h>
#include "mysyslog.h"

#define MSG_BUFFER 4096

int auth_user(const char *login_name) {
    FILE *fp = fopen("/etc/myRPC/users.conf", "r");
    if (!fp) return 0;
    char temp[128];
    while (fgets(temp, sizeof(temp), fp)) {
        temp[strcspn(temp, "\n")] = '\0';
        if (strcmp(temp, login_name) == 0) {
            fclose(fp);
            return 1;
        }
    }
    fclose(fp);
    return 0;
}

char *sanitize_line(char *input_line) {
    while (*input_line == ' ' || *input_line == '\t') input_line++;
    char *endptr = input_line + strlen(input_line) - 1;
    while (endptr > input_line && (*endptr == ' ' || *endptr == '\t')) *endptr-- = 0;
    return input_line;
}

char *quote_shell_arg(const char *src) {
    size_t len = strlen(src);
    char *escaped = malloc(len * 4 + 3);
    if (!escaped) return NULL;
    char *q = escaped;
    *q++ = '\'';
    for (size_t i = 0; i < len; ++i) {
        if (src[i] == '\'') {
            memcpy(q, "'\\\''", 4);
            q += 4;
        } else {
            *q++ = src[i];
        }
    }
    *q++ = '\'';
    *q = '\0';
    return escaped;
}

char *read_file_content(const char *filepath) {
    FILE *f = fopen(filepath, "r");
    if (!f) return strdup("(unreadable)");
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    rewind(f);
    char *buffer = malloc(len + 1);
    if (!buffer) return strdup("(alloc fail)");
    fread(buffer, 1, len, f);
    buffer[len] = '\0';
    fclose(f);
    return buffer;
}

void handle_rpc_json(const char *input_json, char *output_json) {
    struct json_object *json_in = json_tokener_parse(input_json);
    struct json_object *json_out = json_object_new_object();

    if (!json_in) {
        json_object_object_add(json_out, "code", json_object_new_int(400));
        json_object_object_add(json_out, "msg", json_object_new_string("Malformed JSON"));
        strcpy(output_json, json_object_to_json_string(json_out));
        json_object_put(json_out);
        return;
    }

    const char *uname = json_object_get_string(json_object_object_get(json_in, "login"));
    const char *cmd = json_object_get_string(json_object_object_get(json_in, "command"));

    if (!uname || !cmd) {
        json_object_object_add(json_out, "code", json_object_new_int(422));
        json_object_object_add(json_out, "msg", json_object_new_string("Missing fields"));
    } else if (!auth_user(uname)) {
        json_object_object_add(json_out, "code", json_object_new_int(403));
        json_object_object_add(json_out, "msg", json_object_new_string("Unauthorized"));
    } else {
        char temp_path[] = "/tmp/cmdexec_XXXXXX";
        int tmpfd = mkstemp(temp_path);
        if (tmpfd < 0) {
            log_error("mkstemp failed: %s", strerror(errno));
            json_object_object_add(json_out, "code", json_object_new_int(500));
            json_object_object_add(json_out, "msg", json_object_new_string("Temp error"));
        } else {
            close(tmpfd);
            char stdout_file[256], stderr_file[256];
            snprintf(stdout_file, sizeof(stdout_file), "%s.out", temp_path);
            snprintf(stderr_file, sizeof(stderr_file), "%s.err", temp_path);

            char *cmd_safe = quote_shell_arg(cmd);
            if (!cmd_safe) {
                json_object_object_add(json_out, "code", json_object_new_int(500));
                json_object_object_add(json_out, "msg", json_object_new_string("Memory error"));
            } else {
                char sh_cmd[1024];
                snprintf(sh_cmd, sizeof(sh_cmd), "sh -c %s > %s 2> %s", cmd_safe, stdout_file, stderr_file);
                free(cmd_safe);
                int rc = system(sh_cmd);
                char *result_text = read_file_content(rc == 0 ? stdout_file : stderr_file);
                json_object_object_add(json_out, "code", json_object_new_int(rc == 0 ? 0 : 1));
                json_object_object_add(json_out, "msg", json_object_new_string(result_text));
                free(result_text);
            }
        }
    }

    strcpy(output_json, json_object_to_json_string(json_out));
    json_object_put(json_in);
    json_object_put(json_out);
}

int main() {
    log_info("RPC Service Init");

    int port = 1234;
    int tcp_proto = 1;

    FILE *cfgf = fopen("/etc/myRPC/myRPC.conf", "r");
    if (cfgf) {
        char lbuf[256];
        while (fgets(lbuf, sizeof(lbuf), cfgf)) {
            lbuf[strcspn(lbuf, "\n")] = '\0';
            char *entry = sanitize_line(lbuf);
            if (*entry == '#' || !*entry) continue;
            if (strstr(entry, "port")) sscanf(entry, "port = %d", &port);
            else if (strstr(entry, "socket_type")) {
                char t[16];
                if (sscanf(entry, "socket_type = %15s", t) == 1) tcp_proto = strcmp(t, "dgram") != 0;
            }
        }
        fclose(cfgf);
    } else {
        log_warning("No config: %s", strerror(errno));
    }

    int sfd = socket(AF_INET, tcp_proto ? SOCK_STREAM : SOCK_DGRAM, 0);
    if (sfd < 0) {
        log_error("Socket error: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in srv = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
        .sin_addr.s_addr = INADDR_ANY
    };

    if (bind(sfd, (struct sockaddr *)&srv, sizeof(srv)) < 0) {
        log_error("Bind failed: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (tcp_proto && listen(sfd, 5) < 0) {
        log_error("Listen error: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    log_info("Listening on port %d via %s", port, tcp_proto ? "TCP" : "UDP");

    while (1) {
        struct sockaddr_in cli;
        socklen_t clen = sizeof(cli);
        char input_buf[MSG_BUFFER] = {0};

        if (tcp_proto) {
            int csock = accept(sfd, (struct sockaddr *)&cli, &clen);
            if (csock < 0) {
                log_error("Accept failed: %s", strerror(errno));
                continue;
            }
            ssize_t got = recv(csock, input_buf, sizeof(input_buf) - 1, 0);
            if (got <= 0) {
                log_error("TCP read failed: %s", strerror(errno));
                close(csock);
                continue;
            }
            input_buf[got] = '\0';
            char out_json[MSG_BUFFER];
            handle_rpc_json(input_buf, out_json);
            send(csock, out_json, strlen(out_json), 0);
            close(csock);
        } else {
            ssize_t got = recvfrom(sfd, input_buf, sizeof(input_buf) - 1, 0,
                                   (struct sockaddr *)&cli, &clen);
            if (got < 0) {
                log_error("UDP recv failed: %s", strerror(errno));
                continue;
            }
            input_buf[got] = '\0';
            char out_json[MSG_BUFFER];
            handle_rpc_json(input_buf, out_json);
            sendto(sfd, out_json, strlen(out_json), 0, (struct sockaddr *)&cli, clen);
        }
    }

    close(sfd);
    return 0;
}
