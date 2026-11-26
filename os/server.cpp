#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h> 
#include <csignal>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>

volatile sig_atomic_t wasSigHup = 0, wasSigUsr1 = 0, wasSigTerm = 0;

void sigHupHandler(int)  { wasSigHup = 1; }
void sigUsr1Handler(int) { wasSigUsr1 = 1; }
void sigTermHandler(int) { wasSigTerm = 1; }

int main() {
    const int PORT = 1573;

    // 1. Создание и настройка серверного сокета
    int srv = socket(AF_INET, SOCK_STREAM, 0);
    if (srv < 0) {
        perror("socket");
        return 1;
    }

    int on = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &on, sizeof on);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(PORT);

    if (bind(srv, (sockaddr*)&addr, sizeof addr) < 0) {
        perror("bind");
        close(srv);
        return 1;
    }
    if (listen(srv, 5) < 0) {
        perror("listen");  
        close(srv);
        return 1;
    }
    printf("Listening on port %d, pid %d\n", PORT, getpid());

    // 2. Установка обработчиков сигналов
    struct sigaction sa{};
    sa.sa_handler = sigHupHandler;
    sigaction(SIGHUP, &sa, NULL);

    sa.sa_handler = sigUsr1Handler;
    sigaction(SIGUSR1, &sa, NULL);

    sa.sa_handler = sigTermHandler;
    sigaction(SIGTERM, &sa, NULL);

    // 3. Блокировка сигналов вне pselect   
    sigset_t blockedMask, origMask;
    sigemptyset(&blockedMask);
    sigaddset(&blockedMask, SIGHUP);
    sigaddset(&blockedMask, SIGUSR1);
    sigaddset(&blockedMask, SIGTERM);
    sigprocmask(SIG_BLOCK, &blockedMask, &origMask);

    int client = -1;

    // 4. Основной цикл событий
    for (;;) {
        // Проверка флагов сигналов перед уходом в ожидание
        if (wasSigHup) {
            wasSigHup = 0;
            printf("SIGHUP received\n");
        }
        if (wasSigUsr1) {
            wasSigUsr1 = 0;
            printf("SIGUSR1 received\n");
        }
        if (wasSigTerm) {
            printf("SIGTERM, exiting\n");
            break;
        }

        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(srv, &fds);
        int maxFd = srv;

        if (client >= 0) {
            FD_SET(client, &fds);
            if (client > maxFd) {
                maxFd = client;
            }
        }

        int r = pselect(maxFd + 1, &fds, NULL, NULL, NULL, &origMask);
        if (r < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("pselect");
            break;
        }

        // Обработка нового подключения
        if (FD_ISSET(srv, &fds)) {
            sockaddr_in ca{};
            socklen_t len = sizeof ca;
            int fd = accept(srv, (sockaddr*)&ca, &len);
            if (fd >= 0) {
                printf("New connection from %s:%d\n",
                       inet_ntoa(ca.sin_addr), ntohs(ca.sin_port));
                if (client < 0) {
                    client = fd;
                } else {
                    close(fd);
                    printf("Extra connection closed\n");
                }
            } else if (errno != EINTR && errno != EAGAIN) {
                perror("accept");
            }
        }

        // Чтение данных от активного клиента
        if (client >= 0 && FD_ISSET(client, &fds)) {
            char buf[1024];
            ssize_t n = read(client, buf, sizeof buf);
            if (n > 0) {
                printf("Received %zd bytes: ", n);
                fwrite(buf, 1, n, stdout);
                fflush(stdout);
            } else if (n == 0) {
                printf("Client disconnected\n");
                close(client);
                client = -1;
            } else if (errno != EINTR) {
                perror("read");
                close(client);
                client = -1;
            }
        }
    }

    if (client >= 0) {
        close(client);
    }
    close(srv);
    return 0;
}