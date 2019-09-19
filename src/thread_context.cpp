#include "coco/thread_context.h"
#include "coco/scheduler.h"
#include "coco/task.h"

namespace coco {

namespace detail {
static thread_local ThreadContext* __current_thread = nullptr;
}

ThreadContext::ThreadContext(Scheduler* parent, Id id)
    : parent(parent), tid(id), stopped(true), waiting(false)
{
    idle_task = std::make_unique<Task>([] {}, 128);
}

ThreadContext* ThreadContext::get_current_thread()
{
    return detail::__current_thread;
}

void ThreadContext::run()
{
    detail::__current_thread = this;

    stopped = false;
    waiting = false;

    if (run_queue.empty()) {
        wait();

        if (stopped) return;
    }

    current_task = std::move(run_queue.front());
    run_queue.pop();

    switch_to(idle_task.get(), current_task.get());
}

void ThreadContext::gc()
{
    while (!zombie_queue.empty()) {
        zombie_queue.pop();
    }
}

void ThreadContext::queue_task(std::unique_ptr<Task> task)
{
    run_queue.push(std::move(task));
}

void ThreadContext::wait()
{
    gc();

    std::unique_lock<std::mutex> lock(cv_mutex);
    waiting = true;
    cv.wait(lock);
    waiting = false;
}

void ThreadContext::yield() { get_current_thread()->yield_current(); }

void ThreadContext::yield_current()
{
    Task* prev = current_task.get();

    switch (current_task->state) {
    case Task::State::RUNNABLE:
        run_queue.push(std::move(current_task));
        break;
    case Task::State::SLEEPING:
        waiting_queue.push(std::move(current_task));
        break;
    case Task::State::TERMINATED:
        zombie_queue.push(std::move(current_task));
        break;
    }

    Task* next = nullptr;

retry:
    while (run_queue.empty()) {
        wait();

        if (stopped) {
            next = idle_task
                       .get(); // this will send us back to the place where
                               // this->run() is called and continue from there
            break;
        }
    }

    if (!next) {
        if (run_queue.empty()) goto retry;

        current_task = std::move(run_queue.front());
        run_queue.pop();
        next = current_task.get();
    }

    prev = switch_to(prev, next);
}

Task* ThreadContext::switch_to(Task* prev, Task* next)
{
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
