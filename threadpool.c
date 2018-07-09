#include "threadpool.h"

struct thread thread_new() {
    int sockets[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets))
        ERR("socket pair creation failed")

    struct thread th;
    th.socket = sockets[0];
    th.remote_socket = sockets[1];
    th.fd = 0;

    return th;
}

void thread_run(struct thread *th, void *(*fun) (void *)) {
    if (pthread_create(&th->fd, NULL, fun, &th))
        ERR("pthread_create() failed")
}

void thread_send_msg(struct thread *th, struct query* q) {
    if (send(th->socket, q, sizeof(struct query), 0) == 0)
        ERR("send failed")
}

void thread_recv_msg(struct thread *th, struct answer* r) {
    if (recv(th->socket, r, sizeof(struct answer), 0) == 0)
        ERR("send failed")
}

void thread_stop(struct thread *th) {
    struct query q;
    q.state = RunTask;
    q.task = NULL;
    thread_send_msg(th, &q);
}

void thread_destroy(struct thread *p) {
    pthread_cancel(p->fd);
    close(p->socket);
    close(p->remote_socket);
}


struct thread_list thread_list_new(uint32_t len, void *(*fun) (void *)) {
    void* data = malloc(len*sizeof(struct thread));
    if (data == NULL)
        ERR("malloc failed")

    struct thread_list res;
    res.len = len;
    res.threads = data;

    for (uint32_t i = 0; i < len; i++) {
        res.threads[i] = thread_new();
        thread_run(&res.threads[i], fun);
    }

    return res;
}

void thread_list_respawn(struct thread_list *p, uint32_t pos) {
    if (pos >= p->len)
        return;

    thread_destroy(&p->threads[pos]);
    p->threads[pos] = thread_new();
    thread_run(&p->threads[pos], p->fun);
}

int thread_list_create_epoll_queue(struct thread_list *p) {
    int epfd = epoll_create1(0);
    if (epfd == 0)
        ERR("epoll_create failed")

    for (uint32_t i = 0; i < p->len; i++) {
        struct epoll_event ev;
        epoll_data_t val;
        val.u32 = i;
        ev.data = val;
        ev.events = EPOLLIN;
        int res = epoll_ctl(epfd, EPOLL_CTL_ADD, p->threads[i].socket, &ev);
        if (res)
            ERR("epoll_ctl failed");
    }

    return epfd;
}

void thread_list_destroy(struct thread_list *p) {
    for (uint32_t i = 0; i < p->len; i++) {
        thread_destroy(&p->threads[i]);
    }
    free(p->threads);
}