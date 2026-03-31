#ifndef NET_TASKSCHEDULER_H
#define NET_TASKSCHEDULER_H

#include <mutex>
#include <atomic>

#include "Timer.h"
#include "Channel.h"

namespace lsy::net {

class TaskScheduler {
public:
    explicit TaskScheduler(uint32_t id = 0) : id_(id), is_shutdown_(false) {
    }

    virtual ~TaskScheduler() = default;

    void Start() {
        is_shutdown_ = false;
        while (!is_shutdown_) {
            timer_queue_.HandleTimerEvent();
            HandleEvent(10);
        }
    }

    void Stop() {
        is_shutdown_ = true;
    }

    TimerId AddTimer(const TimerEvent &event, uint32_t msec) {
        return timer_queue_.AddTimer(event, msec);
    }

    void RemoveTimer(TimerId timerId) {
        timer_queue_.RemoveTimer(timerId);
    }

    virtual void UpdateChannel(const ChannelPtr &channel) = 0;

    virtual void RemoveChannel(const ChannelPtr &channel) = 0;

    virtual int HandleEvent(int timeout_ms) = 0;

protected:
    uint32_t id_;
    std::atomic_bool is_shutdown_;
    std::mutex mutex_;
    TimerQueue timer_queue_;
};

using TaskSchedulerPtr = std::shared_ptr<TaskScheduler>;

} // lsy::net

#endif // NET_TASKSCHEDULER_H
