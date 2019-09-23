#ifndef _COCO_FUTEX_H_
#define _COCO_FUTEX_H_

#include "coco/thread_context.h"

#include <atomic>

namespace coco {

/* user-mode implementation of Linux's futex(2) */
template <typename T> class Futex {
public:
    void wait(std::atomic<T>& uval, T& old_val)
    {
        Task* task = ThreadContext::get_current_task();
        if (!task) {
            throw std::runtime_error(
                "futex wait can only be used in coroutine context");
        }

        queue_lock.lock();

        unsigned new_val = uval.load(std::memory_order_relaxed);
        if (new_val != old_val) {
            queue_lock.unlock();

            return;
        }

        ThreadContext::set_sleep();
        wait_queue.push(std::make_unique<WaitEntry>(
            ThreadContext::get_current_thread(), task));
        queue_lock.unlock();

        ThreadContext::yield();
    }

    void wake(size_t task_count)
    {
        size_t count = 0;

        queue_lock.lock();

        while (!wait_queue.empty()) {
            auto entry = std::move(wait_queue.front());
            wait_queue.pop();

            entry->thread->wake_up(entry->task);

            if (++count >= task_count) break;
        }

        queue_lock.unlock();
    }

private:
    struct WaitEntry {
        ThreadContext* thread;
        Task* task;

        WaitEntry(ThreadContext* thread, Task* task)
            : thread(thread), task(task)
        {}
    };

    std::queue<std::unique_ptr<WaitEntry>> wait_queue;
    coco::SpinLock queue_lock;
};

} // namespace coco

#endif
