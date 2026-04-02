#ifndef NET_TASKSCHEDULER_H
#define NET_TASKSCHEDULER_H

#include <mutex>
#include <atomic>

#include "Timer.h"
#include "Channel.h"

namespace lsy::net {

/**
 * @brief 任务调度器抽象基类
 * @details 定义IO事件调度与定时器管理的统一接口，基于IO多路复用模型，
 *          封装定时器队列、调度器启停逻辑，子类需实现纯虚的事件监听/处理接口
 */
class TaskScheduler {
public:
    /**
     * @brief 构造函数
     * @param id 调度器唯一标识ID，默认值为0
     * @details 初始化调度器ID、关闭状态标记
     */
    explicit TaskScheduler(uint32_t id = 0) : id_(id), is_shutdown_(false) {
    }

    /**
     * @brief 虚析构函数
     * @details 抽象类默认虚析构，保证子类资源正确释放
     */
    virtual ~TaskScheduler() = default;

    /**
     * @brief 启动任务调度器
     * @details 开启调度循环，循环处理定时器事件与IO多路复用事件，直到调用Stop()停止
     */
    void Start() {
        is_shutdown_ = false;
        while (!is_shutdown_) {
            timer_queue_.HandleTimerEvent();
            HandleEvent(10);
        }
    }

    /**
     * @brief 停止任务调度器
     * @details 设置关闭标记，终止调度循环
     */
    void Stop() {
        is_shutdown_ = true;
    }

    /**
     * @brief 添加定时任务
     * @param event 定时器触发的回调事件
     * @param msec 定时延迟时间（毫秒）
     * @return 定时器唯一ID，用于后续删除定时器
     */
    TimerId AddTimer(const TimerEvent &event, uint32_t msec) {
        return timer_queue_.AddTimer(event, msec);
    }

    /**
     * @brief 移除定时任务
     * @param timerId 待移除的定时器唯一ID
     */
    void RemoveTimer(TimerId timerId) {
        timer_queue_.RemoveTimer(timerId);
    }

    /**
     * @brief 纯虚函数：更新通道事件监听
     * @param channel 待更新的事件通道智能指针
     * @details 子类实现通道的添加/修改/删除逻辑
     */
    virtual void UpdateChannel(const ChannelPtr &channel) = 0;

    /**
     * @brief 纯虚函数：移除通道事件监听
     * @param channel 待移除的事件通道智能指针
     * @details 子类实现通道的注销逻辑
     */
    virtual void RemoveChannel(const ChannelPtr &channel) = 0;

    /**
     * @brief 纯虚函数：处理IO多路复用事件
     * @param timeout_ms 事件监听超时时间（毫秒）
     * @return 触发事件的数量，失败返回负数
     * @details 子类实现底层IO事件监听与分发逻辑
     */
    virtual int HandleEvent(int timeout_ms) = 0;

protected:
    /// 调度器唯一标识ID
    uint32_t id_;
    /// 调度器关闭状态原子标记，多线程安全控制循环启停
    std::atomic_bool is_shutdown_;
    /// 互斥锁，用于保护共享资源操作
    std::mutex mutex_;
    /// 定时器队列，管理所有定时任务的添加/删除/触发
    TimerQueue timer_queue_;
};

/**
 * @brief 任务调度器智能指针类型别名
 */
using TaskSchedulerPtr = std::shared_ptr<TaskScheduler>;

} // lsy::net

#endif // NET_TASKSCHEDULER_H