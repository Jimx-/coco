#ifndef _COCO_SCHEDULER_H_
#define _COCO_SCHEDULER_H_

#include "coco/task.h"
#include "coco/thread_context.h"

#include <thread>

namespace coco {

class Scheduler {
public:
    explicit Scheduler(int nr_threads = 1, uint64_t monitor_tick_us = 10000);

    static Scheduler& get_instance();

    void go(std::function<void()>&& fn, size_t stacksize = 4096);

    void run();
    void stop();

private:
    int nr_threads;
    uint64_t monitor_tick_us;
    bool stopped;
    std::exception_ptr eptr;
    std::vector<std::unique_ptr<ThreadContext>> threads;

    void monitor_thread_func();
};

} // namespace coco

#endif
