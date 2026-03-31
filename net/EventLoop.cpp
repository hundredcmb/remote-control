#include "EventLoop.h"

#include <cassert>

#include "EpollTaskScheduler.h"

namespace lsy::net {

EventLoop::EventLoop(uint32_t num_threads)
    : index_(0), num_threads_(num_threads) {
}

EventLoop::~EventLoop() {
    Quit();
}

TaskSchedulerPtr EventLoop::GetTaskScheduler() {
    assert(index_ >= 0 && !task_schedulers_.empty());
    if (task_schedulers_.empty()) {
        return nullptr;
    }
    if (task_schedulers_.size() == 1) {
        return task_schedulers_[0];
    }
    TaskSchedulerPtr task_scheduler = task_schedulers_[index_];
    if (++index_ >= task_schedulers_.size()) {
        index_ = 0;
    }
    return task_scheduler;
}

TimerId EventLoop::AddTimer(const TimerEvent& timerEvent, uint32_t msec) {
    if (task_schedulers_.empty()) {
        return -1;
    }
    return task_schedulers_[0]->AddTimer(timerEvent, msec);
}

void EventLoop::RemoveTimer(TimerId timerId) {
    if (!task_schedulers_.empty()) {
        task_schedulers_[0]->RemoveTimer(timerId);
    }
}

void EventLoop::UpdateChannel(const ChannelPtr& channel) {
    if (!task_schedulers_.empty()) {
        task_schedulers_[0]->UpdateChannel(channel);
    }
}

void EventLoop::RemoveChannel(const ChannelPtr &channel) {
    if (!task_schedulers_.empty()) {
        task_schedulers_[0]->RemoveChannel(channel);
    }
}

void EventLoop::Loop() {
    if (!task_schedulers_.empty()) {
        return;
    }

    for (uint32_t i = 0; i < num_threads_; ++i) {
        TaskSchedulerPtr task_scheduler(new EpollTaskScheduler(i));
        task_schedulers_.emplace_back();
        ThreadPtr t = std::make_shared<std::thread>([task_scheduler]() -> void {
            task_scheduler->Start();
        });
        // pthread_t nativeHandle = t->native_handle();
        threads_.emplace_back(std::move(t));
    }
}

void EventLoop::Quit() {
    for (TaskSchedulerPtr &task_scheduler : task_schedulers_) {
        task_scheduler->Stop();
    }
    task_schedulers_.clear();
    for (ThreadPtr &thread : threads_) {
        if (thread->joinable()) {
            thread->join();
        }
    }
    threads_.clear();
}

} // lsy::net
