#include "coco/task.h"
#include "coco/scheduler.h"

namespace coco {

Task::Task(std::function<void()>&& func, size_t stacksize)
    : func(func), stacksize(stacksize), state(State::RUNNABLE)
{
    init_stack(stacksize);
}

void Task::init_stack(size_t stacksize)
{
    stack = std::make_unique<uint8_t[]>(stacksize);
    auto stacktop = stack.get() + stacksize;

    regs = (StackFrame*)(stacktop - sizeof(StackFrame));

    /* setup the return address */
    reg_t* return_address = (reg_t*)((uint8_t*)regs - sizeof(reg_t));
    *return_address = (reg_t)&Task::run;

    /* setup the initial stack pointer */
    regs->rsp = (reg_t)return_address - sizeof(reg_t); // make room for rbp

    /* setup this pointer(only works for System V amd64 ABI) */
    regs->rdi = (reg_t)this;
}

void Task::run(Task* task)
{
    task->func();

    task->state = State::TERMINATED;
    SCHED.yield();
}

} // namespace coco
