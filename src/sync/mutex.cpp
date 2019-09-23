#include "coco/sync/mutex.h"

namespace coco {

enum LockState {
    UNLOCKED = 0,
    LOCKED_UNCONTENTED = 1,
    LOCKED_CONTENTED = 2,
};

Mutex::Mutex() { state.store(UNLOCKED); }

bool Mutex::try_lock()
{
    uint8_t old_state = UNLOCKED;
    return state.compare_exchange_strong(old_state, LOCKED_UNCONTENTED,
                                         std::memory_order_acquire,
                                         std::memory_order_relaxed);
}

void Mutex::lock()
{
    if (try_lock()) return;

    uint8_t locked_contented = (uint8_t)LOCKED_CONTENTED;
    while (state.exchange(locked_contented, std::memory_order_acquire) !=
           UNLOCKED) {
        waiting_queue.wait(state, locked_contented);
    }
}

void Mutex::unlock()
{
    if (state.exchange(UNLOCKED, std::memory_order_release) ==
        LOCKED_CONTENTED) {
        waiting_queue.wake(1);
    }
}

} // namespace coco
