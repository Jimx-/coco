#ifndef _COCO_SCHEDULER_H_
#define _COCO_SCHEDULER_H_

#include "coco/task.h"
#include "coco/thread_context.h"

namespace coco {

class Scheduler {
public:
    explicit Scheduler(int nr_threads = 1);

    static Scheduler& get_instance();

    void go(std::function<void()>&& fn, size_t stacksize = 4096);

    void run();

private:
    int nr_threads;
    std::vector<std::unique_ptr<ThreadContext>> threads;
};

} // namespace coco

#endif
