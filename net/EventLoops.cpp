#include "EventLoops.h"

#include "EpollTaskScheduler.h"

namespace lsy::net {

EventLoops::EventLoops(uint32_t num_threads)
    : next_index_(1),
     num_threads_(num_threads < 2 ? 2 : num_threads),
     is_started_(false) {
}

EventLoops::~EventLoops() {
    Quit();
}

TaskSchedulerPtr EventLoops::GetAcceptTaskScheduler() {
    if (!is_started_) {
        return nullptr;
    }
    return task_schedulers_[0];
}

TaskSchedulerPtr EventLoops::GetNextIoTaskScheduler() {
    if (!is_started_) {
        return nullptr;
    }
    TaskSchedulerPtr task_scheduler = task_schedulers_[next_index_];
    if (++next_index_ >= task_schedulers_.size()) {
        next_index_ = 1;
    }
    return task_scheduler;
}

void EventLoops::Loop() {
    if (is_started_) {
        return;
    }
    for (uint32_t i = 0; i < num_threads_; ++i) {
        TaskSchedulerPtr task_scheduler(new EpollTaskScheduler(i));
        task_schedulers_.emplace_back(task_scheduler);
        ThreadPtr t = std::make_shared<std::thread>([task_scheduler]() -> void {
            task_scheduler->Start();
        });
        // pthread_t native_handle = t->native_handle();
        threads_.emplace_back(std::move(t));
    }
    is_started_ = true;
}

void EventLoops::Quit() {
    if (!is_started_) {
        return;
    }
    for (TaskSchedulerPtr &task_scheduler: task_schedulers_) {
        task_scheduler->Stop();
    }
    task_schedulers_.clear();
    for (ThreadPtr &thread: threads_) {
        if (thread->joinable()) {
            thread->join();
        }
    }
    threads_.clear();
}

} // lsy::net
