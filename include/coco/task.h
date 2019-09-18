#ifndef _COCO_TASK_H_
#define _COCO_TASK_H_

#include "coco/stackframe.h"

#include <cstdint>
#include <functional>
#include <memory>

namespace coco {

class Task {
    friend class ThreadContext;

public:
    enum class State {
        RUNNABLE,
        SLEEPING,
        TERMINATED,
    };

    Task(std::function<void()>&& func, size_t stacksize);

private:
    State state;
    std::unique_ptr<uint8_t[]> stack;
    size_t stacksize;
    StackFrame* regs;
    std::function<void()> func;

    void init_stack(size_t stacksize);
    static void run(Task* task);
};

} // namespace coco

#endif
