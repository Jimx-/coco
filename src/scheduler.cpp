#include "coco/scheduler.h"

namespace coco {

Scheduler::Scheduler() : idle_task([] {}, 128) { current_task = &idle_task; }

Scheduler& Scheduler::get_instance()
{
    static Scheduler sched;
    return sched;
}

void Scheduler::go(std::function<void()>&& fn, size_t stacksize)
{
    tasks.emplace_back(std::make_unique<Task>(std::move(fn), stacksize));
    run_queue.push(tasks.back().get());
}

void Scheduler::run()
{
    if (run_queue.empty()) return;

    auto task = run_queue.front();
    run_queue.pop();

    switch_to(current_task, task);
}

void Scheduler::yield()
{
    switch (current_task->state) {
    case Task::State::RUNNABLE:
        run_queue.push(current_task);
        break;
    case Task::State::SLEEPING:
        break;
    case Task::State::TERMINATED:
        break;
    }

    Task* next = nullptr;
    if (run_queue.empty()) {
        next = &idle_task; // this will send us back to the caller of SCHED::run
    } else {
        next = run_queue.front();
        run_queue.pop();
    }

    Task* prev = switch_to(current_task, next);
}

Task* Scheduler::switch_to(Task* prev, Task* next)
{
    current_task = next;
    __asm__ volatile("mov %0, %%rax\n\t"
                     "mov %%rsp, (%1)\n\t" // save prev stack pointer
                     "mov %1, %%rsp\n\t"
                     "push %%rbx\n\t" // save prev registers
                     "push %%rbp\n\t"
                     "push %%r12\n\t"
                     "push %%r13\n\t"
                     "push %%r14\n\t"
                     "push %%r15\n\t"
                     "push %%rdi\n\t"
                     "mov %2, %%rsp\n\t"
                     "pop %%rdi\n\t" // restore next registers
                     "pop %%r15\n\t"
                     "pop %%r14\n\t"
                     "pop %%r13\n\t"
                     "pop %%r12\n\t"
                     "pop %%rbp\n\t"
                     "pop %%rbx\n\t"
                     "pop %%rsp\n\t"
                     "retq" ::"r"(prev),
                     "r"((uint8_t*)prev->regs + sizeof(StackFrame) -
                         sizeof(reg_t)), // rsp in prev stack frame
                     "r"(next->regs)
                     : "rax", "rdi", "memory");
}

} // namespace coco
