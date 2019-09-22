#include "coco/scheduler.h"

#include <chrono>
#include <map>

namespace coco {

Scheduler::Scheduler(int nr_threads, uint64_t monitor_tick_us)
    : nr_threads(nr_threads), monitor_tick_us(monitor_tick_us), stopped(true),
      eptr(nullptr)
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
    eptr = nullptr;

    for (int i = 1; i < nr_threads; i++) {
        ThreadContext::Id tid = threads.size();
        threads.push_back(std::make_unique<ThreadContext>(this, tid));

        auto* thread = threads.back().get();
        native_threads.emplace_back([this, thread] {
            try {
                thread->run();
            } catch (...) {
                this->eptr = std::current_exception();
                this->stop();
            }
        });
    }

    native_threads.emplace_back(
        std::bind(&Scheduler::monitor_thread_func, this));

    try {
        main_thread->run();
    } catch (...) {
        eptr = std::current_exception();
        stop();
    }

    for (auto&& t : native_threads) {
        t.join();
    }

    threads.clear();
    threads.push_back(std::make_unique<ThreadContext>(this, 0));

    if (eptr) {
        std::rethrow_exception(eptr);
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
        size_t empty_count = 0;
        for (auto&& p : threads) {
            total_load += p->run_queue_size();
            load_map.emplace(p->run_queue_size(), p.get());

            if (p->is_waiting()) {
                if (p->empty()) {
                    empty_count++;
                    continue;
                }

                if (!p->run_queue_size()) {
                    p->poll_io();
                }

                if (p->has_runnable()) {
                    /* wake up waiting threads with tasks to run */
                    p->notify();
                }
            }
        }

        size_t avg_load = total_load / threads.size();

        if (empty_count == threads.size()) {
            /* no more task to be done */
            stop();
            break;
        }

        /* steal tasks from threads with heavier load */
        if (!load_map.begin()->first) {
            auto waiting_range = load_map.equal_range(0);
            size_t waiting_count =
                std::distance(waiting_range.first, waiting_range.second);

            auto it = load_map.rbegin();
            while (it != load_map.rend() && it->first > avg_load) {
                size_t n = it->first - avg_load;
                std::vector<std::unique_ptr<Task>> tasks;
                it->second->steal_tasks(n, tasks);

                size_t avg_tasks =
                    tasks.size() /
                    std::distance(waiting_range.first, waiting_range.second);
                if (!avg_tasks) avg_tasks = 1;

                auto tp = tasks.begin();
                for (auto p = waiting_range.first; p != waiting_range.second;
                     p++) {
                    size_t np = std::distance(tp, tasks.end());
                    if (np > avg_tasks) np = avg_tasks;
                    if (!np) break;

                    while (np--) {
                        p->second->queue_task(std::move(*tp++));
                    }
                }

                it++;
            }

            for (auto p = waiting_range.first; p != waiting_range.second; p++) {
                if (p->second->run_queue_size()) p->second->notify();
            }
        }
    }
}

} // namespace coco
