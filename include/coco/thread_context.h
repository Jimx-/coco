#ifndef _COCO_THREAD_CONTEXT_H_
#define _COCO_THREAD_CONTEXT_H_

#include "coco/spinlock.h"
#include "coco/task.h"

#include <condition_variable>
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
    size_t run_queue_size() const { return run_queue.size(); }
    static ThreadContext* get_current_thread();
    bool is_waiting() const { return waiting; }

    void queue_task(std::unique_ptr<Task> task);
    void steal_tasks(size_t n, std::vector<std::unique_ptr<Task>>& tasks);
    void notify();

    void run();
    void stop();

    void gc();

    static void yield();

private:
    Scheduler* parent;
    Id tid;
    bool stopped;
    bool waiting;
    std::exception_ptr eptr;

    Task idle_task;

    std::unique_ptr<Task> current_task;
    std::queue<std::unique_ptr<Task>> run_queue;
    std::queue<std::unique_ptr<Task>> waiting_queue;
    std::queue<std::unique_ptr<Task>> zombie_queue;
    SpinLock run_queue_lock;

    std::mutex cv_mutex;
    std::condition_variable cv;

    void wait();

    void yield_current();
    __attribute__((naked)) Task* switch_to(Task* prev, Task* next);
};

} // namespace coco

#endif
