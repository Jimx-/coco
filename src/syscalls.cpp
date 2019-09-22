#include "coco/syscalls.h"
#include "coco/io_context.h"
#include "coco/thread_context.h"

#include <dlfcn.h>
#include <fcntl.h>
#include <poll.h>
#include <stdarg.h>

#include <iostream>

namespace coco {

static void init_hook();

static int __poll(struct pollfd* fds, nfds_t nfds, int timeout)
{
    auto task = ThreadContext::get_current_task();

    if (!task || timeout == 0) {
        return poll_f(fds, nfds, timeout);
    }

    int retval = poll_f(fds, nfds, 0);
    if (retval) return retval;

    auto revents =
        std::make_unique<short[]>(nfds); // alive for as long as we are sleeping

    bool added = false;
    for (int i = 0; i < nfds; i++) {
        struct pollfd* p = &fds[i];
        if (p->fd < 0) continue;
        p->revents = 0;

        bool ret = ThreadContext::get_current_io_poller()->add(
            p->fd, p->events, task, &revents.get()[i]);

        added |= ret;
    }

    if (!added) {
        for (int i = 0; i < nfds; i++) {
            fds[i].revents = POLLNVAL;
        }
        errno = 0;
        return nfds;
    }

    ThreadContext::sleep();

    int n = 0;
    for (int i = 0; i < nfds; i++) {
        fds[i].revents = revents[i];
        if (fds[i].revents) n++;
    }
    errno = 0;
    return n;
}

template <typename F, typename... Args>
static typename std::result_of<F(int, Args...)>::type safe_rdwt(F fn, int fd,
                                                                Args&&... args)
{
    int retval;
    do {
        retval = fn(fd, std::forward<Args>(args)...);
    } while (retval == -1 && errno == EINTR);

    return retval;
}

template <typename F, typename... Args>
static typename std::result_of<F(int, Args...)>::type
do_rdwt(int fd, F fn, short event, int timeout, ssize_t count, Args&&... args)
{
    auto task = ThreadContext::get_current_task();

    if (!task) {
        return fn(fd, std::forward<Args>(args)...);
    }

    struct pollfd fds;
    fds.fd = fd;
    fds.events = event;
    fds.revents = 0;

retry:
    int retval = __poll(&fds, 1, timeout);
    if (retval == -1) {
        if (errno == EINTR) {
            goto retry;
        }
        return -1;
    }

    if (retval == 0) {
        errno = EAGAIN;
        return -1;
    }

    return safe_rdwt(fn, fd, std::forward<Args>(args)...);
}

} // namespace coco

extern "C"
{
    open_t open_f = nullptr;
    pipe_t pipe_f = nullptr;
    read_t read_f = nullptr;
    write_t write_f = nullptr;
    poll_t poll_f = nullptr;

    int open(const char* pathname, int flags, ...)
    {
        if (!open_f) coco::init_hook();

        va_list parg;
        int retval;

        va_start(parg, flags);

        if (flags & O_CREAT) {
            mode_t mode = va_arg(parg, mode_t);
            retval = open_f(pathname, flags, mode);
        } else {
            retval = open_f(pathname, flags);
        }

        va_end(parg);

        if (retval >= 0) {
            coco::IOContext::get_instance().create_pfd(retval);
        }

        return retval;
    }

    int pipe(int pipefd[2])
    {
        if (!pipe_f) coco::init_hook();

        int retval = pipe_f(pipefd);
        if (!retval) {
            coco::IOContext::get_instance().create_pfd(pipefd[0]);
            coco::IOContext::get_instance().create_pfd(pipefd[1]);
        }

        return retval;
    }

    ssize_t read(int fd, void* buf, size_t count)
    {
        if (!read_f) coco::init_hook();
        return coco::do_rdwt(fd, read_f, POLLIN, -1, count, buf, count);
    }

    ssize_t write(int fd, const void* buf, size_t count)
    {
        if (!write_f) coco::init_hook();
        return coco::do_rdwt(fd, write_f, POLLOUT, -1, count, buf, count);
    }
}

namespace coco {

namespace detail {

static void init_hook()
{
    open_f = (open_t)dlsym(RTLD_NEXT, "open");
    pipe_f = (pipe_t)dlsym(RTLD_NEXT, "pipe");
    read_f = (read_t)dlsym(RTLD_NEXT, "read");
    write_f = (write_t)dlsym(RTLD_NEXT, "write");
    poll_f = (poll_t)dlsym(RTLD_NEXT, "poll");
}

} // namespace detail

static void init_hook()
{
    static bool inited = false;
    if (!inited) {
        detail::init_hook();
        inited = true;
    }
}

} // namespace coco
