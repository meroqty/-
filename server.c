#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

enum { PORT = 5000, BACKLOG = 16, BUF_SIZE = 4096 };

struct client {
    int                fd;
    struct sockaddr_in addr;
};

static pthread_mutex_t stats_m = PTHREAD_MUTEX_INITIALIZER;
static int             active  = 0;

static int stats_add(int delta)
{
    pthread_mutex_lock(&stats_m);
    int now = (active += delta);
    pthread_mutex_unlock(&stats_m);
    return now;
}

static int send_all(int fd, const char *buf, size_t len)
{
    while (len > 0) {
        ssize_t n = send(fd, buf, len, 0);
        if (n == -1) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        buf += n;
        len -= (size_t)n;
    }
    return 0;
}

static void *worker(void *arg)
{
    struct client *c = arg;
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &c->addr.sin_addr, ip, sizeof ip);
    int port = ntohs(c->addr.sin_port);

    printf("[+] %s:%d подключён,  активных: %d\n", ip, port, stats_add(+1));

    char buf[BUF_SIZE];
    for (;;) {
        ssize_t n = recv(c->fd, buf, sizeof buf, 0);
        if (n == -1 && errno == EINTR)
            continue;
        if (n <= 0)                   /* 0 — клиент закрыл, -1 — ошибка */
            break;
        if (send_all(c->fd, buf, (size_t)n) == -1)
            break;
    }

    close(c->fd);
    printf("[-] %s:%d отключён,   активных: %d\n", ip, port, stats_add(-1));
    free(c);
    return NULL;
}

int main(void)
{
    signal(SIGPIPE, SIG_IGN);

    int srv = socket(AF_INET, SOCK_STREAM, 0);
    if (srv == -1) {
        perror("socket");
        return EXIT_FAILURE;
    }

    int yes = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);

    struct sockaddr_in addr = {
        .sin_family      = AF_INET,
        .sin_port        = htons(PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };
    if (bind(srv, (struct sockaddr *)&addr, sizeof addr) == -1) {
        perror("bind");
        return EXIT_FAILURE;
    }
    if (listen(srv, BACKLOG) == -1) {
        perror("listen");
        return EXIT_FAILURE;
    }
    printf("Сервер слушает порт %d\n", PORT);

    for (;;) {
        struct sockaddr_in peer;
        socklen_t len = sizeof peer;
        int fd = accept(srv, (struct sockaddr *)&peer, &len);
        if (fd == -1) {
            if (errno == EINTR)
                continue;
            perror("accept");
            break;
        }

        struct client *c = malloc(sizeof *c);
        if (c == NULL) {
            close(fd);
            continue;
        }
        c->fd   = fd;
        c->addr = peer;

        pthread_t tid;
        int err = pthread_create(&tid, NULL, worker, c);
        if (err != 0) {
            fprintf(stderr, "pthread_create: %s\n", strerror(err));
            close(fd);
            free(c);
            continue;
        }
        pthread_detach(tid);
    }

    close(srv);
    return EXIT_FAILURE;
}