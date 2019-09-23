#include "coco/sync/shared_mutex.h"

namespace coco {

#define RWS_PD_WRITERS (1 << 0)
#define RWS_PD_READERS (1 << 1)
#define RCNT_SHIFT 2
#define RCNT_INC_STEP (1 << RCNT_SHIFT)
#define RWS_WRLOCKED (1 << 31)

#define RW_RDLOCKED(l) ((l) >= RCNT_INC_STEP)
#define RW_WRLOCKED(l) ((l)&RWS_WRLOCKED)

static bool __can_rdlock(uint32_t state) { return !RW_WRLOCKED(state); }
static bool __can_wrlock(uint32_t state)
{
    return !(RW_WRLOCKED(state) || RW_RDLOCKED(state));
}

SharedMutex::SharedMutex()
{
    state.store(0);
    pending_reader_count = 0;
    pending_writer_count = 0;
    pending_reader_serial.store(0);
    pending_writer_serial.store(0);
}

bool SharedMutex::try_lock_shared()
{
again:
    auto old_state = state.load(std::memory_order_relaxed);

    while (__can_rdlock(old_state)) {
        uint32_t new_state = old_state + RCNT_INC_STEP;
        if (!RW_RDLOCKED(new_state)) goto again;

        if (state.compare_exchange_strong(old_state, new_state,
                                          std::memory_order_acquire,
                                          std::memory_order_relaxed))
            return true;
    }

    return false;
}

void SharedMutex::lock_shared()
{
    while (true) {
        if (try_lock_shared()) return;

        auto old_state = state.load(std::memory_order_relaxed);
        if (__can_rdlock(old_state)) continue;

        pending_mutex.lock();
        pending_reader_count++;

        old_state = state.fetch_or(RWS_PD_READERS, std::memory_order_relaxed);
        unsigned int old_serial =
            pending_reader_serial.load(std::memory_order_relaxed);
        pending_mutex.unlock();

        if (!__can_rdlock(old_state)) {
            pending_reader_wq.wait(pending_reader_serial, old_serial);
        }

        pending_mutex.lock();
        pending_reader_count--;
        if (pending_reader_count == 0) {
            state.fetch_and(~RWS_PD_READERS, std::memory_order_relaxed);
        }
        pending_mutex.unlock();
    }
}

void SharedMutex::unlock_shared() { unlock(); }

bool SharedMutex::try_lock()
{
    auto old_state = state.load(std::memory_order_relaxed);

    while (__can_wrlock(old_state)) {
        if (state.compare_exchange_strong(old_state, old_state | RWS_WRLOCKED,
                                          std::memory_order_acquire,
                                          std::memory_order_relaxed))
            return true;
    }

    return false;
}

void SharedMutex::lock()
{
    while (true) {
        if (try_lock()) return;

        auto old_state = state.load(std::memory_order_relaxed);
        if (__can_wrlock(old_state)) continue;

        pending_mutex.lock();
        pending_writer_count++;

        old_state = state.fetch_or(RWS_PD_WRITERS, std::memory_order_relaxed);
        unsigned int old_serial =
            pending_writer_serial.load(std::memory_order_relaxed);
        pending_mutex.unlock();

        if (!__can_rdlock(old_state)) {
            pending_writer_wq.wait(pending_writer_serial, old_serial);
        }

        pending_mutex.lock();
        pending_writer_count--;
        if (pending_writer_count == 0) {
            state.fetch_and(~RWS_PD_WRITERS, std::memory_order_relaxed);
        }
        pending_mutex.unlock();
    }
}

void SharedMutex::unlock()
{
    auto old_state = state.load(std::memory_order_relaxed);

    if (RW_WRLOCKED(old_state)) {
        old_state = state.fetch_and(~RWS_WRLOCKED, std::memory_order_release);

        if (!(old_state & RWS_PD_WRITERS) && !(old_state & RWS_PD_READERS))
            return;
    } else if (RW_RDLOCKED(old_state)) {
        old_state = state.fetch_sub(RCNT_INC_STEP, std::memory_order_release);

        if ((old_state >> RCNT_SHIFT) != 1 ||
            !(old_state & RWS_PD_WRITERS && old_state & RWS_PD_READERS))
            return;
    } else {
        return;
    }

    pending_mutex.lock();
    if (pending_writer_count) {
        pending_writer_serial++;
        pending_mutex.unlock();

        pending_writer_wq.wake(1);
    } else if (pending_reader_count) {
        pending_reader_serial++;
        pending_mutex.unlock();

        pending_reader_wq.wake(SIZE_MAX);
    } else {
        pending_mutex.unlock();
    }
}

} // namespace coco
