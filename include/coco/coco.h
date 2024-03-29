#ifndef _COCO_H_
#define _COCO_H_

#include "coco/scheduler.h"
#include "coco/thread_context.h"

namespace coco {

extern void go(std::function<void()>&& fn, size_t stacksize = 1 * 1024 * 1024);
extern void run();
extern void yield();

} // namespace coco

#endif
