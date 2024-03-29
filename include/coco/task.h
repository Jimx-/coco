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

    void set_state(State state) { this->state = state; }

    bool check_stack_overflow();

private:
    static const size_t STACK_GUARD_SIZE = 0x1000;
    State state;
    std::unique_ptr<uint8_t[]> stack;
    size_t stacksize;
    StackFrame* regs;
    std::function<void()> func;
    std::exception_ptr eptr;

    void init_stack(size_t stacksize);
    static void run(Task* task);
};

} // namespace coco

#endif
