#ifndef NET_EVENTLOOP_H
#define NET_EVENTLOOP_H

#include "Noncopyable.h"
#include "TaskScheduler.h"

namespace lsy::net {

using ThreadPtr = std::shared_ptr<std::thread>;

class EventLoops : Noncopyable {
public:
    explicit EventLoops(uint32_t num_threads);

    virtual ~EventLoops();

    TaskSchedulerPtr GetAcceptTaskScheduler();

    TaskSchedulerPtr GetNextIoTaskScheduler();

    void Loop();

    void Quit();

private:
    uint32_t num_threads_;
    uint32_t next_index_;
    std::atomic_bool is_started_;
    std::vector<TaskSchedulerPtr> task_schedulers_;
    std::vector<ThreadPtr> threads_;
};

} // lsy::net

#endif // NET_EVENTLOOP_H
