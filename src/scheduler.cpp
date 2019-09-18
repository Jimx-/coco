#include "coco/scheduler.h"

namespace coco {

Scheduler::Scheduler(int nr_threads) : nr_threads(nr_threads)
{
    threads.push_back(std::make_unique<ThreadContext>(this, 0));
}

Scheduler& Scheduler::get_instance()
{
    static Scheduler sched;
    return sched;
}

void Scheduler::go(std::function<void()>&& fn, size_t stacksize)
{
    auto thread = ThreadContext::get_current_thread();
    auto new_task = std::make_unique<Task>(std::move(fn), stacksize);

    if (thread) {
        thread->queue_task(std::move(new_task));
        return;
    }

    threads.front()->queue_task(std::move(new_task));
}

void Scheduler::run()
{
    auto* main_thread = threads.front().get();

    main_thread->run();
}

} // namespace coco
