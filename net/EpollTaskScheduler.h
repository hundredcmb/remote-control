#ifndef NET_EPOLLTASKSCHEDULER_H
#define NET_EPOLLTASKSCHEDULER_H

#include <sys/epoll.h>
#include <unistd.h>

#include <cstring>
#include <cassert>

#include "TaskScheduler.h"

namespace lsy::net {

using EpollFd = int;
using EpollOperation = int;

class EpollTaskScheduler : public TaskScheduler {
public:
    explicit EpollTaskScheduler(uint32_t id) : TaskScheduler(id) {
        epollfd_ = ::epoll_create1(EPOLL_CLOEXEC);
        if (epollfd_ < 0) {
            fprintf(stderr, "epoll_create1 EPOLL_CLOEXEC error: %s\n",
                    strerror(errno));
            throw std::runtime_error("EpollTaskScheduler create failed");
        }
    }

    ~EpollTaskScheduler() override {
        if (epollfd_ >= 0) {
            ::close(epollfd_);
        }
    }

    void UpdateChannel(const ChannelPtr &channel) override {
        std::lock_guard lock(mutex_);
        if (!channel) {
            return;
        }
        int fd = channel->GetFd();
        auto it = channels_.find(fd);
        if (it != channels_.end()) {
            if (channel->IsNoneEvent()) {
                Update(EPOLL_CTL_DEL, channel);
                channels_.erase(it);
            } else {
                Update(EPOLL_CTL_MOD, channel);
            }
        } else if (!channel->IsNoneEvent()) {
            Update(EPOLL_CTL_ADD, channel);
            channels_.emplace(fd, channel);
        }
    }

    void RemoveChannel(const ChannelPtr &channel) override {
        std::lock_guard lock(mutex_);
        if (!channel) {
            return;
        }
        int fd = channel->GetFd();
        auto it = channels_.find(fd);
        if (it != channels_.end()) {
            Update(EPOLL_CTL_DEL, channel);
            channels_.erase(it);
        }
    }

    int HandleEvent(int timeout_ms) override {
        ::epoll_event events[kMaxEvents]{};
        int nfds = ::epoll_wait(epollfd_, events, kMaxEvents, timeout_ms);
        if (nfds < 0) {
            fprintf(stderr, "epoll_wait error: %s\n", strerror(errno));
            return nfds;
        }
        for (int i = 0; i < nfds; ++i) {
            auto *channel = static_cast<Channel *>(events[i].data.ptr);
            assert(channel);
            channel->HandleEvent(events[i].events);
        }
        return nfds;
    }

private:
    void Update(EpollOperation operation, const ChannelPtr &channel) const {
        assert(channel);
        ::epoll_event event{};
        if (operation != EPOLL_CTL_DEL) {
            event.data.ptr = channel.get();
            event.events = channel->GetEvents();
        }
        int ret = ::epoll_ctl(epollfd_, operation, channel->GetFd(),
                              &event);
        if (ret < 0) {
            fprintf(stderr, "epoll_ctl error: %s\n", strerror(errno));
        }
    }

    EpollFd epollfd_ = -1;
    std::mutex mutex_;
    std::unordered_map<FileDescriptor, ChannelPtr> channels_;
    static constexpr int kMaxEvents = 256;
};

} // lsy::net

#endif // NET_EPOLLTASKSCHEDULER_H
