#include "coco/thread_context.h"
#include "coco/scheduler.h"
#include "coco/task.h"

namespace coco {

namespace detail {
static thread_local ThreadContext* __current_thread = nullptr;
}

ThreadContext::ThreadContext(Scheduler* parent, Id id)
    : parent(parent), tid(id), stopped(false), waiting(false),
      idle_task([] {}, 128), eptr(nullptr), io_poller(this)
{}

ThreadContext* ThreadContext::get_current_thread()
{
    return detail::__current_thread;
}

Task* ThreadContext::get_current_task()
{
    auto tc = get_current_thread();
    return tc ? tc->current_task.get() : nullptr;
}

IOPoller* ThreadContext::get_current_io_poller()
{
    auto tc = get_current_thread();
    return tc ? tc->get_io_poller() : nullptr;
}

void ThreadContext::run()
{
    detail::__current_thread = this;

    waiting = false;
    eptr = nullptr;

    run_queue_lock.lock();

    if (run_queue.empty()) {
        run_queue_lock.unlock();
        wait();

        if (stopped) return;

        run_queue_lock.lock();
    }

    current_task = std::move(run_queue.front());
    run_queue.pop();
    run_queue_lock.unlock();

    switch_to(&idle_task, current_task.get());

    detail::__current_thread = nullptr;

    if (eptr) {
        std::rethrow_exception(eptr);
    }
}

void ThreadContext::stop()
{

    std::lock_guard<std::mutex> lock(cv_mutex);
    stopped = true;
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

void ThreadContext::steal_tasks(size_t n,
                                std::vector<std::unique_ptr<Task>>& tasks)
{
    run_queue_lock.lock();
    while (!run_queue.empty() && tasks.size() < n) {
        tasks.emplace_back(std::move(run_queue.front()));
        run_queue.pop();
    }
    run_queue_lock.unlock();
}

void ThreadContext::notify()
{
    if (waiting) {
        cv.notify_all();
    }
}

void ThreadContext::poll_io() { io_poller.poll(); }

void ThreadContext::wait()
{
    gc();

    std::unique_lock<std::mutex> lock(cv_mutex);
    if (stopped) return;
    waiting = true;
    cv.wait(lock);
    waiting = false;
}

void ThreadContext::yield() { get_current_thread()->yield_current(); }
void ThreadContext::sleep() { get_current_thread()->sleep_current(); }

void ThreadContext::yield_current()
{
    Task* prev = current_task.get();
    Task* next = nullptr;

    if (current_task->state == Task::State::TERMINATED &&
        current_task->eptr != nullptr) {
        eptr = current_task->eptr;
        stopped = true;
    }

    if (stopped) {
        next = &idle_task; // this will send us back to the place where
                           // this->run() is called and continue from there
    }

    if (!next) {
        run_queue_lock.lock();

    retry:
        while (run_queue.empty()) {
            if (current_task->state == Task::State::RUNNABLE) {
                next = current_task.get();
                break;
            }

            io_poller.poll();

            if (!has_runnable()) {
                run_queue_lock.unlock();
                wait();
                run_queue_lock.lock();
            }

            if (stopped) {
                next = &idle_task;
                break;
            }
        }

        if (!next) {
            if (run_queue.empty()) goto retry;

            /* return the current task to the proper queue */
            /* this can only be done here because we are still running in
             * current task's context so we have to make sure the current task
             * is not released before we are ready to switch to the next task's
             * context */
            switch (current_task->state) {
            case Task::State::RUNNABLE:
                run_queue.push(std::move(current_task));
                break;
            case Task::State::SLEEPING:
                waiting_queue.push_back(std::move(current_task));
                break;
            case Task::State::TERMINATED:
                zombie_queue.push(std::move(current_task));
                break;
            }

            current_task = std::move(run_queue.front());
            run_queue.pop();
            next = current_task.get();
        }

        run_queue_lock.unlock();
    }

    prev = switch_to(prev, next);

    if (prev->state != Task::State::TERMINATED) {
        if (__builtin_expect(prev->check_stack_overflow(), false)) {
            /* we are throwing an exception on the next task's stack so it will
             * be catched by Task::run() and cause the next task to enter this
             * function again with terminated state */
            throw std::runtime_error("stack overflow detected in coroutine");
        }
    }
}

void ThreadContext::sleep_current()
{
    current_task->state = Task::State::SLEEPING;
    yield_current();
}

void ThreadContext::wake_up(Task* task)
{
    /* this is only called by own thread or the monitor thread(when own thread
     * is waiting) so there is no need to acquire any lock */
    if (task == current_task.get()) {
        current_task->state = Task::State::RUNNABLE;
    } else {
        for (auto it = waiting_queue.begin(); it != waiting_queue.end(); it++) {
            if (it->get() == task) {
                (*it)->state = Task::State::RUNNABLE;
                run_queue.push(std::move(*it));
                waiting_queue.erase(it);
                break;
            }
        }
    }
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
