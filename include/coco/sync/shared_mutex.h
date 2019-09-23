#ifndef _COCO_SHARED_MUTEX_H_
#define _COCO_SHARED_MUTEX_H_

#include "coco/sync/mutex.h"

namespace coco {

class SharedMutex {
public:
    SharedMutex();

    bool try_lock_shared();
    void lock_shared();
    void unlock_shared();

    bool try_lock();
    void lock();
    void unlock();

private:
    std::atomic<uint32_t> state;

    Mutex pending_mutex;
    unsigned int pending_reader_count;
    unsigned int pending_writer_count;

    Futex<uint32_t> pending_reader_wq;
    std::atomic<uint32_t> pending_reader_serial;
    Futex<uint32_t> pending_writer_wq;
    std::atomic<uint32_t> pending_writer_serial;
};

} // namespace coco

#endif
