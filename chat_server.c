#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#include <time.h>

#define MAX_CLIENTS 100
#define PORT 9000

struct Client {
    int fd;
    int registered;
    char id[64];
    char name[64];
};

int main() {
    setenv("TZ", "Asia/Ho_Chi_Minh", 1);
    tzset();

    int listener = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    bind(listener, (struct sockaddr*)&server_addr, sizeof(server_addr));
    listen(listener, 10);

    struct Client clients[MAX_CLIENTS];
    struct pollfd fds[MAX_CLIENTS + 1];

    fds[0].fd = listener;
    fds[0].events = POLLIN;
    int nfds = 1;

    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].fd = -1;
        fds[i + 1].fd = -1;
    }

    char *prompt = "client_id: client_name\n";

    while (1) {
        if (poll(fds, nfds, -1) < 0) break;

        if (fds[0].revents & POLLIN) {
            int new_fd = accept(listener, NULL, NULL);
            for (int i = 1; i <= MAX_CLIENTS; i++) {
                if (fds[i].fd == -1) {
                    fds[i].fd = new_fd;
                    fds[i].events = POLLIN;
                    clients[i - 1].fd = new_fd;
                    clients[i - 1].registered = 0;
                    if (i + 1 > nfds) nfds = i + 1;
                    break;
                }
            }
            send(new_fd, prompt, strlen(prompt), 0);
        }

        for (int i = 1; i < nfds; i++) {
            if (fds[i].fd != -1 && (fds[i].revents & POLLIN)) {
                char buffer[2048];
                memset(buffer, 0, sizeof(buffer));
                int bytes = recv(fds[i].fd, buffer, sizeof(buffer) - 1, 0);

                int c_idx = i - 1;

                if (bytes <= 0) {
                    close(fds[i].fd);
                    fds[i].fd = -1;
                    clients[c_idx].fd = -1;
                } else {
                    buffer[strcspn(buffer, "\r\n")] = 0;
                    if (strlen(buffer) == 0) continue;

                    if (!clients[c_idx].registered) {
                        char id[64], name[64];
                        if (sscanf(buffer, "%63[^:]: %63s", id, name) == 2) {
                            strcpy(clients[c_idx].id, id);
                            strcpy(clients[c_idx].name, name);
                            clients[c_idx].registered = 1;
                        } else {
                            send(fds[i].fd, prompt, strlen(prompt), 0);
                        }
                    } else {
                        time_t now = time(NULL);
                        struct tm *t = localtime(&now);
                        char time_str[64];
                        strftime(time_str, sizeof(time_str), "%Y/%m/%d %I:%M:%S%p", t);

                        char send_buf[3000];
                        snprintf(send_buf, sizeof(send_buf), "%s %s: %s\n", time_str, clients[c_idx].id, buffer);

                        for (int j = 0; j < MAX_CLIENTS; j++) {
                            if (clients[j].fd != -1 && clients[j].registered && clients[j].fd != fds[i].fd) {
                                send(clients[j].fd, send_buf, strlen(send_buf), 0);
                            }
                        }
                    }
                }
            }
        }
    }
    close(listener);
    return 0;
}
