#ifndef NET_EPOLLTASKSCHEDULER_H
#define NET_EPOLLTASKSCHEDULER_H

#include <sys/epoll.h>
#include <unistd.h>

#include <cstring>
#include <cassert>

#include "TaskScheduler.h"

namespace lsy::net {

/**
 * @brief epoll文件描述符类型别名
 */
using EpollFd = int;

/**
 * @brief epoll操作类型别名（EPOLL_CTL_ADD/MOD/DEL）
 */
using EpollOperation = int;

/**
 * @brief 基于epoll的任务调度器
 * @details 继承自TaskScheduler，实现Linux epoll IO多路复用模型，
 *          负责管理Channel的事件注册、修改、删除，监听并分发epoll事件，
 *          保证多线程环境下的操作安全
 */
class EpollTaskScheduler : public TaskScheduler {
public:
    /**
     * @brief 构造函数
     * @param id 调度器唯一标识ID
     * @details 创建带EPOLL_CLOEXEC标志的epoll文件描述符，创建失败则抛出运行时异常
     */
    explicit EpollTaskScheduler(uint32_t id) : TaskScheduler(id) {
        epollfd_ = ::epoll_create1(EPOLL_CLOEXEC);
        if (epollfd_ < 0) {
            fprintf(stderr, "epoll_create1 EPOLL_CLOEXEC error: %s\n",
                    strerror(errno));
            throw std::runtime_error("EpollTaskScheduler create failed");
        }
    }

    /**
     * @brief 析构函数
     * @details 关闭epoll文件描述符，释放系统资源
     */
    ~EpollTaskScheduler() override {
        if (epollfd_ >= 0) {
            ::close(epollfd_);
        }
    }

    /**
     * @brief 更新通道事件监听
     * @param channel 待更新的事件通道智能指针
     * @details 根据通道当前事件状态，执行epoll的添加/修改/删除操作，维护通道映射表
     */
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

    /**
     * @brief 移除通道事件监听
     * @param channel 待移除的事件通道智能指针
     * @details 从epoll中删除通道监听，同时从通道映射表中移除
     */
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

    /**
     * @brief 处理epoll监听事件
     * @param timeout_ms epoll_wait超时时间（毫秒），-1表示永久阻塞
     * @return 触发事件的文件描述符数量，失败返回-1
     * @details 等待epoll事件触发，遍历并分发事件到对应Channel处理
     */
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
    /**
     * @brief 封装epoll_ctl操作
     * @param operation epoll操作类型（EPOLL_CTL_ADD/MOD/DEL）
     * @param channel 目标事件通道智能指针
     * @details 执行底层epoll事件注册/修改/删除，操作失败打印错误信息
     */
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

    /// epoll实例文件描述符
    EpollFd epollfd_ = -1;
    /// 互斥锁，保证多线程下通道操作的线程安全
    std::mutex mutex_;
    /// 文件描述符 -> 事件通道的映射表，管理所有注册的通道
    std::unordered_map<FileDescriptor, ChannelPtr> channels_;
    /// epoll_wait最大监听事件数量
    static constexpr int kMaxEvents = 256;
};

} // lsy::net

#endif // NET_EPOLLTASKSCHEDULER_H