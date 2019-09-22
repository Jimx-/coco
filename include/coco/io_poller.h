#ifndef _COCO_IO_POLLER_H_
#define _COCO_IO_POLLER_H_

#include "coco/io_context.h"
#include "coco/task.h"

namespace coco {

class ThreadContext;

class IOPoller {
public:
    IOPoller(ThreadContext* parent);

    bool add(int fd, short events, Task* task, short* revents);
    void poll();

private:
    ThreadContext* parent;
    IOContext* io_ctx;
    int epfd;

    void wait_and_process(int timeout);
};

}; // namespace coco

#endif
