#include "coco/io_poller.h"

#include <poll.h>
#include <sys/epoll.h>

namespace coco {

#define MAX_EVENTS 1024

static int get_epoll_events(short poll_events)
{
    int retval = 0;

    if (poll_events & POLLIN) {
        retval |= EPOLLIN;
    }
    if (poll_events & POLLOUT) {
        retval |= EPOLLOUT;
    }
    if (poll_events & POLLERR) {
        retval |= EPOLLERR;
    }

    return retval;
}

static short get_poll_events(int epoll_events)
{
    short retval = 0;

    if (epoll_events & EPOLLIN) {
        retval |= POLLIN;
    }
    if (epoll_events & EPOLLOUT) {
        retval |= POLLOUT;
    }
    if (epoll_events & EPOLLERR) {
        retval |= POLLERR;
    }

    return retval;
}

IOPoller::IOPoller(ThreadContext* parent) : parent(parent)
{
    io_ctx = &IOContext::get_instance();

    epfd = epoll_create1(EPOLL_CLOEXEC);
    if (epfd == -1) {
        throw std::runtime_error("failed to create epoll fd");
    }
}

bool IOPoller::add(int fd, short events, Task* task, short* revents)
{
    auto pfd = io_ctx->get_pfd(fd);
    if (!pfd) {
        return false;
    }

    short old_events;
    pfd->add(events, task, revents, old_events);

    if (old_events != events) {
        struct epoll_event evt;
        int op = (old_events) ? EPOLL_CTL_MOD : EPOLL_CTL_ADD;
        evt.data.fd = fd;
        evt.events = get_epoll_events(events) | EPOLLET;

        int retval = epoll_ctl(epfd, op, fd, &evt);
        if (retval) {
            return false;
        }
    }

    return true;
}

void IOPoller::poll() { wait_and_process(0); }

void IOPoller::wait_and_process(int timeout)
{
    struct epoll_event evts[MAX_EVENTS];
    int n = epoll_wait(epfd, evts, MAX_EVENTS, timeout);

    for (int i = 0; i < n; i++) {
        struct epoll_event* evt = &evts[i];
        int fd = evt->data.fd;
        auto pfd = io_ctx->get_pfd(fd);
        if (!pfd) continue;

        short events = get_poll_events(evt->events);
        short old_events;
        pfd->notify(parent, events, old_events);

        if (old_events != events) {
            struct epoll_event del;
            int op = (events) ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
            del.data.fd = fd;
            del.events = get_epoll_events(events) | EPOLLET;

            int retval = epoll_ctl(epfd, op, fd, &del);
        }
    }
}

} // namespace coco
