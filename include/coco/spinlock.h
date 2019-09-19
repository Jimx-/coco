#ifndef _COCO_SPINLOCK_H_
#define _COCO_SPINLOCK_H_

#include <atomic>

namespace coco {

class SpinLock {
public:
    SpinLock() : flag(ATOMIC_FLAG_INIT) {}

    void lock()
    {
        while (flag.test_and_set(std::memory_order_acquire))
            ;
    }

    bool try_lock() { return !flag.test_and_set(std::memory_order_release); }

    void unlock() { flag.clear(std::memory_order_release); }

private:
    std::atomic_flag flag;
};

} // namespace coco

#endif
