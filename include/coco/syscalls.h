#ifndef _COCO_SYSCALLS_H_
#define _COCO_SYSCALLS_H_

#include <cstddef>
#include <poll.h>
#include <unistd.h>

extern "C"
{
    typedef int (*open_t)(const char* pathname, int flags, ...);
    extern open_t open_f;

    typedef int (*pipe_t)(int pipefd[2]);
    extern pipe_t pipe_f;

    typedef ssize_t (*read_t)(int fd, void* buf, size_t size);
    extern read_t read_f;

    typedef ssize_t (*write_t)(int fd, const void* buf, size_t size);
    extern write_t write_f;

    typedef int (*poll_t)(struct pollfd* fds, nfds_t nfds, int timeout);
    extern poll_t poll_f;
}

#endif
