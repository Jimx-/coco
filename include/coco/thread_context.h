#ifndef _COCO_THREAD_CONTEXT_H_
#define _COCO_THREAD_CONTEXT_H_

#include <cstddef>
#include <memory>
#include <queue>
#include <vector>

namespace coco {

class Scheduler;
class Task;

class ThreadContext {
public:
    using Id = size_t;

    ThreadContext(Scheduler* parent, Id id);

    Id get_tid() const { return tid; }
    static ThreadContext* get_current_thread();

    void queue_task(std::unique_ptr<Task> task);

    void run();

    static void yield();

private:
    Scheduler* parent;
    Id tid;

    std::unique_ptr<Task> idle_task;
    std::vector<std::unique_ptr<Task>> tasks;

    Task* current_task;
    std::queue<Task*> run_queue;
    std::queue<Task*> waiting_queue;

    void yield_current();
    __attribute__((naked)) Task* switch_to(Task* prev, Task* next);
};

} // namespace coco

#endif
