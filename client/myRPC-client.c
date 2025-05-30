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

#define MAX_DATA 4096

void usage_guide(const char *progname) {
    printf("Usage: %s [OPTIONS]\n", progname);
    printf("Options:\n");
    printf("  -x, --exec \"cmd\"       Command to execute\n");
    printf("  -a, --addr \"ip\"        Target IP address\n");
    printf("  -n, --net PORT         Target port\n");
    printf("  -t, --tcp              Use TCP socket\n");
    printf("  -u, --udp              Use UDP socket\n");
    printf("      --help             Display help info\n");
}

int main(int argc, char *argv[]) {
    char *cmdline = NULL, *ip_addr = NULL;
    int netport = 0, tcp_flag = 0, udp_flag = 0;

    static struct option opts[] = {
        {"exec", required_argument, 0, 'x'},
        {"addr", required_argument, 0, 'a'},
        {"net", required_argument, 0, 'n'},
        {"tcp", no_argument, 0, 't'},
        {"udp", no_argument, 0, 'u'},
        {"help", no_argument, 0, 0},
        {0, 0, 0, 0}
    };

    int ch, idx = 0;
    while ((ch = getopt_long(argc, argv, "x:a:n:tu", opts, &idx)) != -1) {
        switch (ch) {
            case 'x': cmdline = strdup(optarg); break;
            case 'a': ip_addr = strdup(optarg); break;
            case 'n': netport = atoi(optarg); break;
            case 't': tcp_flag = 1; break;
            case 'u': udp_flag = 1; break;
            case 0:
                if (strcmp(opts[idx].name, "help") == 0) {
                    usage_guide(argv[0]);
                    return 0;
                }
                break;
            default:
                usage_guide(argv[0]);
                return EXIT_FAILURE;
        }
    }

    if (!cmdline || !ip_addr || !netport || (!tcp_flag && !udp_flag)) {
        usage_guide(argv[0]);
        return EXIT_FAILURE;
    }

    struct passwd *user_info = getpwuid(getuid());
    if (!user_info) {
        perror("user lookup");
        return EXIT_FAILURE;
    }
    const char *login = user_info->pw_name;

    struct json_object *payload = json_object_new_object();
    json_object_object_add(payload, "login", json_object_new_string(login));
    json_object_object_add(payload, "command", json_object_new_string(cmdline));
    const char *request = json_object_to_json_string(payload);

    int connfd;
    struct sockaddr_in target;
    connfd = socket(AF_INET, (tcp_flag ? SOCK_STREAM : SOCK_DGRAM), 0);
    if (connfd < 0) {
        perror("sock create");
        return EXIT_FAILURE;
    }

    target.sin_family = AF_INET;
    target.sin_port = htons(netport);
    inet_pton(AF_INET, ip_addr, &target.sin_addr);

    if (tcp_flag) {
        if (connect(connfd, (struct sockaddr*)&target, sizeof(target)) < 0) {
            perror("tcp connect");
            close(connfd);
            return EXIT_FAILURE;
        }

        send(connfd, request, strlen(request), 0);
        char response[MAX_DATA] = {0};
        ssize_t rlen = recv(connfd, response, sizeof(response) - 1, 0);
        if (rlen > 0) {
            response[rlen] = '\0';
            printf("Server reply: %s\n", response);
        } else {
            perror("tcp recv");
        }

    } else {
        struct timeval timeout = {5, 0};
        setsockopt(connfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

        sendto(connfd, request, strlen(request), 0, (struct sockaddr*)&target, sizeof(target));
        char response[MAX_DATA] = {0};
        socklen_t tlen = sizeof(target);
        ssize_t rlen = recvfrom(connfd, response, sizeof(response) - 1, 0, (struct sockaddr*)&target, &tlen);
        if (rlen > 0) {
            response[rlen] = '\0';
            printf("Server reply: %s\n", response);
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                fprintf(stderr, "UDP response timeout\n");
            else
                perror("udp recvfrom");
        }
    }

    close(connfd);
    free(cmdline);
    free(ip_addr);
    json_object_put(payload);

    return 0;
}
