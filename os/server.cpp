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


// g++ -std=c++17 server.cpp -o server 
//./server
volatile sig_atomic_t wasSigHup = 0;
void sigHupHandler(int) { wasSigHup = 1; }

int main() {
    const int PORT = 8080;

    // 1. Сокет
    int srv = socket(AF_INET, SOCK_STREAM, 0);
    if (srv < 0) { perror("socket"); return 1; }
    int on = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &on, sizeof on);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(PORT);
    if (bind(srv, (sockaddr*)&addr, sizeof addr) < 0) { perror("bind"); return 1; }
    if (listen(srv, 5) < 0) { perror("listen"); return 1; }
    printf("Listening on port %d, pid %d\n", PORT, getpid());

    // 2. Обработчик сигнала
    struct sigaction sa;
    sigaction(SIGHUP, NULL, &sa);
    sa.sa_handler = sigHupHandler;
    sa.sa_flags |= SA_RESTART;
    sigaction(SIGHUP, &sa, NULL);

    // 3. Блокировка сигнала
    sigset_t blockedMask, origMask;
    sigemptyset(&blockedMask);
    sigaddset(&blockedMask, SIGHUP);
    sigprocmask(SIG_BLOCK, &blockedMask, &origMask);

    int client = -1;   // единственное принятое соединение

    // 4. Основной цикл
    for (;;) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(srv, &fds);
        int maxFd = srv;
        if (client >= 0) {
            FD_SET(client, &fds);
            if (client > maxFd) maxFd = client;
        }

        int r = pselect(maxFd + 1, &fds, NULL, NULL, NULL, &origMask);
        if (r < 0) {
            if (errno == EINTR) {
                if (wasSigHup) {
                    wasSigHup = 0;
                    printf("SIGHUP received\n");
                }
                continue;
            }
            perror("pselect");
            break;
        }

        if (FD_ISSET(srv, &fds)) {
            sockaddr_in ca;
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

        if (client >= 0 && FD_ISSET(client, &fds)) {
            char buf[1024];
            ssize_t n = read(client, buf, sizeof buf);
            if (n > 0) {
                printf("Received %zd bytes\n", n);
            } else {
                if (n == 0) printf("Client disconnected\n");
                else perror("read");
                close(client);
                client = -1;
            }
        }
    }
    close(srv);
    return 0;
}