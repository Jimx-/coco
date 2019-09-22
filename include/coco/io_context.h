#ifndef _COCO_IO_CONTEXT_H_
#define _COCO_IO_CONTEXT_H_

#include <memory>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

namespace coco {

class Task;
class ThreadContext;

struct PollEntry {
    Task* task;
    short* revents;

    PollEntry(Task* task, short* revents) : task(task), revents(revents) {}
};

class PollableFileDesc {
public:
    PollableFileDesc(int fd) : fd(fd), events(0) {}

    void add(short& new_events, Task* task, short* revents, short& old_events);
    void notify(ThreadContext* thread, short& events, short& old_events);

private:
    std::mutex mutex;

    int fd;
    short events;

    using EntryList = std::vector<std::unique_ptr<PollEntry>>;
    EntryList in_list;
    EntryList out_list;
    EntryList in_out_list;
    EntryList err_list;

    void wake_up_list(ThreadContext* thread, EntryList PollableFileDesc::*list,
                      short revents);
};

using PPFd = std::shared_ptr<PollableFileDesc>;

class IOContext {
public:
    static IOContext& get_instance();

    void create_pfd(int fd);
    PPFd get_pfd(int fd);

private:
    void close_pfd(PollableFileDesc* pfd);
    std::unordered_map<int, PPFd> pfd_map;
    std::shared_mutex pfd_mutex;
};

} // namespace coco

#endif
