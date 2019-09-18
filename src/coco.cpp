#include "coco/coco.h"

namespace coco {

void go(std::function<void()>&& fn, size_t stacksize)
{
    Scheduler::get_instance().go(std::move(fn), stacksize);
}

void run() { Scheduler::get_instance().run(); }

void yield() { ThreadContext::yield(); }

} // namespace coco
