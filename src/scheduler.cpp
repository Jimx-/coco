#include "coco/scheduler.h"

#include <chrono>
#include <map>

namespace coco {

Scheduler::Scheduler(int nr_threads, uint64_t monitor_tick_us)
    : nr_threads(nr_threads), monitor_tick_us(monitor_tick_us), stopped(true)
{
    threads.push_back(std::make_unique<ThreadContext>(this, 0));
}

Scheduler& Scheduler::get_instance()
{
    static Scheduler sched(2);
    return sched;
}

void Scheduler::go(std::function<void()>&& fn, size_t stacksize)
{
    auto thread = ThreadContext::get_current_thread();
    auto new_task = std::make_unique<Task>(std::move(fn), stacksize);

    if (thread) {
        thread->queue_task(std::move(new_task));
        return;
    }

    threads.front()->queue_task(std::move(new_task));
}

void Scheduler::run()
{
    auto* main_thread = threads.front().get();
    std::vector<std::thread> native_threads;

    stopped = false;

    for (int i = 1; i < nr_threads; i++) {
        ThreadContext::Id tid = threads.size();
        threads.push_back(std::make_unique<ThreadContext>(this, tid));

        auto* thread = threads.back().get();
        native_threads.emplace_back([thread] { thread->run(); });
    }

    if (nr_threads > 1) {
        native_threads.emplace_back(
            std::bind(&Scheduler::monitor_thread_func, this));
    }

    main_thread->run();

    for (auto&& t : native_threads) {
        t.join();
    }
}

void Scheduler::stop()
{
    for (auto&& p : threads) {
        p->stop();
        p->notify();
    }

    stopped = true;
}

void Scheduler::monitor_thread_func()
{
    while (!stopped) {
        std::this_thread::sleep_for(std::chrono::microseconds(monitor_tick_us));

        std::multimap<size_t, ThreadContext*> load_map;
        size_t total_load = 0;
        size_t waiting_count = 0;
        for (auto&& p : threads) {
            total_load += p->run_queue_size();
            load_map.emplace(p->run_queue_size(), p.get());

            if (p->is_waiting()) {
                waiting_count++;
                if (p->run_queue_size() > 0) {
                    /* wake up waiting threads with tasks to run */
                    p->notify();
                }
            }
        }

        size_t avg_load = total_load / threads.size();

        if (total_load == 0 && waiting_count == threads.size()) {
            /* no more task to be done */
            stop();
            break;
        }

        if (!load_map.begin()->first) {
            auto waiting_range = load_map.equal_range(0);
        }
    }
}

} // namespace coco
