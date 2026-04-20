#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <poll.h>

#define MAX_CLIENTS 100
#define PORT 9001

struct Client {
    int fd;
    int logged_in;
};

int check_login(const char *user, const char *pass) {
    FILE *f = fopen("users.txt", "r");
    if (!f) return 0;
    char f_user[256], f_pass[256];
    while (fscanf(f, "%255s %255s", f_user, f_pass) == 2) {
        if (strcmp(f_user, user) == 0 && strcmp(f_pass, pass) == 0) {
            fclose(f);
            return 1;
        }
    }
    fclose(f);
    return 0;
}

int main() {
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

    char *login_prompt = "Enter user and pass: ";
    char *login_ok = "Login successful. Enter commands:\n";
    char *login_fail = "Login failed. Try again:\n";

    while (1) {
        if (poll(fds, nfds, -1) < 0) break;

        if (fds[0].revents & POLLIN) {
            int new_fd = accept(listener, NULL, NULL);
            for (int i = 1; i <= MAX_CLIENTS; i++) {
                if (fds[i].fd == -1) {
                    fds[i].fd = new_fd;
                    fds[i].events = POLLIN;
                    clients[i - 1].fd = new_fd;
                    clients[i - 1].logged_in = 0;
                    if (i + 1 > nfds) nfds = i + 1;
                    break;
                }
            }
            send(new_fd, login_prompt, strlen(login_prompt), 0);
        }

        for (int i = 1; i < nfds; i++) {
            if (fds[i].fd != -1 && (fds[i].revents & POLLIN)) {
                char buffer[1024];
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

                    if (!clients[c_idx].logged_in) {
                        char user[256], pass[256];
                        if (sscanf(buffer, "%255s %255s", user, pass) == 2) {
                            if (check_login(user, pass)) {
                                clients[c_idx].logged_in = 1;
                                send(fds[i].fd, login_ok, strlen(login_ok), 0);
                            } else {
                                send(fds[i].fd, login_fail, strlen(login_fail), 0);
                            }
                        } else {
                            send(fds[i].fd, login_prompt, strlen(login_prompt), 0);
                        }
                    } else {
                        char cmd[1050];
                        snprintf(cmd, sizeof(cmd), "%s > out.txt 2>&1", buffer);
                        system(cmd);

                        FILE *f = fopen("out.txt", "r");
                        if (f) {
                            char file_buf[2048];
                            int n;
                            while ((n = fread(file_buf, 1, sizeof(file_buf), f)) > 0) {
                                send(fds[i].fd, file_buf, n, 0);
                            }
                            fclose(f);
                        }
                    }
                }
            }
        }
    }
    close(listener);
    return 0;
}
