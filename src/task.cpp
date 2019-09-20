#include "coco/task.h"
#include "coco/scheduler.h"
#include <iostream>
namespace coco {

Task::Task(std::function<void()>&& func, size_t stacksize)
    : func(func), stacksize(stacksize), state(State::RUNNABLE), eptr(nullptr)
{
    init_stack(stacksize);
}

void Task::init_stack(size_t stacksize)
{
    stack = std::make_unique<uint8_t[]>(stacksize);
    auto stacktop = stack.get() + stacksize;

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

} // namespace coco
