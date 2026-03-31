#ifndef NET_EVENTLOOP_H
#define NET_EVENTLOOP_H

#include "Noncopyable.h"
#include "TaskScheduler.h"

namespace lsy::net {

using ThreadPtr = std::shared_ptr<std::thread>;

class EventLoop : Noncopyable {
public:
    explicit EventLoop(uint32_t num_threads = 1);

    virtual ~EventLoop();

    TaskSchedulerPtr GetTaskScheduler();

    TimerId AddTimer(const TimerEvent& timerEvent, uint32_t msec);

    void RemoveTimer(TimerId timerId);

    void UpdateChannel(const ChannelPtr& channel);

    void RemoveChannel(const ChannelPtr &channel);

    void Loop();

    void Quit();

private:
    uint32_t num_threads_ = 1;
    uint32_t index_ = 1;
    std::vector<TaskSchedulerPtr> task_schedulers_;
    std::vector<ThreadPtr> threads_;
};

} // lsy::net

#endif // NET_EVENTLOOP_H
