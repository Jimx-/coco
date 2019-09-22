#include "coco/io_context.h"
#include "coco/thread_context.h"

#include <poll.h>

namespace coco {

void PollableFileDesc::add(short& new_events, Task* task, short* revents,
                           short& old_events)
{
    std::lock_guard<std::mutex> lock(mutex);

    auto entry = std::make_unique<PollEntry>(task, revents);
    if ((new_events & (POLLIN | POLLOUT)) == (POLLIN | POLLOUT)) {
        in_out_list.emplace_back(std::move(entry));
    } else if (new_events & POLLIN) {
        in_list.emplace_back(std::move(entry));
    } else if (new_events & POLLOUT) {
        out_list.emplace_back(std::move(entry));
    } else {
        err_list.emplace_back(std::move(entry));
    }

    old_events = events;
    new_events = new_events & (POLLIN | POLLOUT);
    if (!new_events) new_events = POLLERR;

    new_events = new_events | old_events;
    events = new_events;
}

void PollableFileDesc::notify(ThreadContext* thread, short& events,
                              short& old_events)
{
    std::lock_guard<std::mutex> lock(mutex);

    short err_events = POLLERR | POLLHUP | POLLNVAL;
    short pending_events = 0;

    short check_events = POLLIN | err_events;
    if (events & check_events) {
        wake_up_list(thread, &PollableFileDesc::in_list, check_events);
    } else if (!in_list.empty()) {
        pending_events |= POLLIN;
    }

    check_events = POLLOUT | err_events;
    if (events & check_events) {
        wake_up_list(thread, &PollableFileDesc::out_list, check_events);
    } else if (!out_list.empty()) {
        pending_events |= POLLOUT;
    }

    check_events = POLLIN | POLLOUT | err_events;
    if (events & check_events) {
        wake_up_list(thread, &PollableFileDesc::in_out_list, check_events);
    } else if (!in_out_list.empty()) {
        pending_events |= (POLLIN | POLLOUT);
    }

    check_events = err_events;
    if (events & check_events) {
        wake_up_list(thread, &PollableFileDesc::err_list, check_events);
    } else if (!err_list.empty()) {
        pending_events |= POLLERR;
    }

    old_events = events;
    this->events = events = pending_events;
}

void PollableFileDesc::wake_up_list(ThreadContext* thread,
                                    EntryList PollableFileDesc::*list,
                                    short revents)
{
    for (auto&& entry : this->*list) {
        *entry->revents = revents;
        thread->wake_up(entry->task);
    }

    (this->*list).clear();
}

IOContext& IOContext::get_instance()
{
    static IOContext io_ctx;
    return io_ctx;
}

void IOContext::create_pfd(int fd)
{
    PPFd old_pfd;
    {
        std::unique_lock<std::shared_mutex> lock(pfd_mutex);
        auto it = pfd_map.find(fd);
        auto new_pfd = std::make_shared<PollableFileDesc>(fd);

        if (it == pfd_map.end()) {
            pfd_map.emplace(fd, std::move(new_pfd));
            return;
        }

        old_pfd = std::move(it->second);
        it->second = std::move(new_pfd);
    }

    if (old_pfd) {
        close_pfd(old_pfd.get());
    }
}

PPFd IOContext::get_pfd(int fd)
{
    std::shared_lock<std::shared_mutex> lock(pfd_mutex);
    auto it = pfd_map.find(fd);
    if (it == pfd_map.end()) {
        return nullptr;
    }

    return it->second;
}

void IOContext::close_pfd(PollableFileDesc* pfd) {}

} // namespace coco
