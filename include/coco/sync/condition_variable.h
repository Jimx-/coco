#ifndef _COCO_CONDITION_VARIABLE_H_
#define _COCO_CONDITION_VARIABLE_H_

#include "coco/sync/futex.h"

#include <atomic>

namespace coco {

class ConditionVariableAny {
public:
    ConditionVariableAny() { state.store(0); }

    template <typename Lock> inline void wait(Lock& lock) { do_wait(lock); }

    void inline notify_one() { signal(1); }
    void inline notify_all() { signal(SIZE_MAX); }

private:
    Futex<unsigned int> waiting_queue;
    std::atomic<unsigned int> state;

    template <typename Lock> void do_wait(Lock& lock)
    {
        unsigned int old_state = state.load(std::memory_order_relaxed);
        lock.unlock();

        waiting_queue.wait(state, old_state);

        lock.lock();
    }

    void signal(size_t task_count)
    {
        state++;

        waiting_queue.wake(task_count);
    }
};

} // namespace coco

#endif
