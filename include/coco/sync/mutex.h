#ifndef _COCO_MUTEX_H_
#define _COCO_MUTEX_H_

#include "coco/sync/futex.h"

namespace coco {

class Mutex {
public:
    Mutex();

    bool try_lock();
    void lock();
    void unlock();

private:
    Futex<uint8_t> waiting_queue;
    std::atomic<uint8_t> state;
};

} // namespace coco

#endif
