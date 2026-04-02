#ifndef NET_EVENTLOOPTHREADPOOL_H
#define NET_EVENTLOOPTHREADPOOL_H

#include "Noncopyable.h"
#include "EpollTaskScheduler.h"

namespace lsy::net {

using ThreadPtr = std::shared_ptr<std::thread>;

class EventLoopThreadPool : Noncopyable {
public:
    explicit EventLoopThreadPool(uint32_t num_threads)
        : next_index_(num_threads > 1 ? 1 : 0),
          num_threads_(num_threads > 1 ? num_threads : 1),
          is_started_(false) {
        for (uint32_t i = 0; i < num_threads_; ++i) {
            task_schedulers_.emplace_back(new EpollTaskScheduler(i));
        }
    }

    virtual ~EventLoopThreadPool() {
        Quit();
    }

    TaskSchedulerPtr GetAcceptTaskScheduler() {
        return task_schedulers_[0];
    }

    TaskSchedulerPtr GetNextIoTaskScheduler() {
        if (num_threads_ == 1) {
            return task_schedulers_[0];
        }
        TaskSchedulerPtr task_scheduler = task_schedulers_[next_index_];
        if (++next_index_ >= task_schedulers_.size()) {
            next_index_ = 1;
        }
        return task_scheduler;
    }

    void Loop() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (is_started_) {
                return;
            }
            for (uint32_t i = 1; i < num_threads_; ++i) {
                TaskSchedulerPtr task_scheduler = task_schedulers_[i];
                ThreadPtr t = std::make_shared<std::thread>(
                    [task_scheduler]() -> void {
                        task_scheduler->Start();
                    }
                );
                threads_.emplace_back(std::move(t));
            }
            is_started_ = true;
        }
        task_schedulers_[0]->Start();
    }

private:
    void Quit() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!is_started_) {
            return;
        }
        for (TaskSchedulerPtr &task_scheduler: task_schedulers_) {
            task_scheduler->Stop();
        }
        for (ThreadPtr &thread: threads_) {
            if (thread->joinable()) {
                thread->join();
            }
        }
        threads_.clear();
        is_started_ = false;
    }

    uint32_t num_threads_;
    uint32_t next_index_;
    bool is_started_;
    std::mutex mutex_;
    std::vector<TaskSchedulerPtr> task_schedulers_;
    std::vector<ThreadPtr> threads_;
};

using EventLoopThreadPoolPtr = std::shared_ptr<EventLoopThreadPool>;

} // lsy::net

#endif // NET_EVENTLOOPTHREADPOOL_H
