#include "coco/task.h"
#include "coco/scheduler.h"

namespace coco {

Task::Task(std::function<void()>&& func, size_t stacksize)
    : func(func), stacksize(stacksize), state(State::RUNNABLE), eptr(nullptr)
{
    init_stack(stacksize);
}

void Task::init_stack(size_t stacksize)
{
    stack = std::make_unique<uint8_t[]>(stacksize + STACK_GUARD_SIZE);
    auto stacktop = stack.get() + stacksize + STACK_GUARD_SIZE;

    regs = (StackFrame*)(stacktop - sizeof(StackFrame));

    /* setup the return address */
    reg_t return_address = (reg_t)regs - sizeof(reg_t);
    /* fix stack alignment(16 bytes at retq) */
    if (return_address & 0xf) {
        return_address -= (return_address & 0xf);
    }
    *(reg_t*)return_address = (reg_t)&Task::run;

    /* setup the initial stack pointer */
    regs->rsp = return_address;

    /* setup this pointer(only works for System V amd64 ABI) */
    regs->rdi = (reg_t)this;
}

void Task::run(Task* task)
{
    try {
        task->func();
    } catch (...) {
        task->eptr = std::current_exception();
    }

    task->state = State::TERMINATED;
    ThreadContext::yield();
}

bool Task::check_stack_overflow()
{
    reg_t sp = regs->rsp - (reg_t)stack.get();

    return !((sp >= STACK_GUARD_SIZE) &&
             (sp <= STACK_GUARD_SIZE + stacksize - sizeof(StackFrame)));
}

} // namespace coco
