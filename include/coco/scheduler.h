#ifndef _COCO_SCHEDULER_H_
#define _COCO_SCHEDULER_H_

#include "coco/task.h"

#include <queue>

namespace coco {

#define SCHED coco::Scheduler::get_instance()

class Scheduler {
public:
    Scheduler();

    static Scheduler& get_instance();

    void go(std::function<void()>&& fn, size_t stacksize = 4096);

    void run();

    void yield();

private:
    Task idle_task;
    std::vector<std::unique_ptr<Task>> tasks;

    Task* current_task;
    std::queue<Task*> run_queue;
    std::queue<Task*> waiting_queue;

    __attribute__((naked)) Task* switch_to(Task* prev, Task* next);
};

} // namespace coco

#endif
