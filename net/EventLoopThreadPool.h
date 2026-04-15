#ifndef NET_EVENTLOOPTHREADPOOL_H
#define NET_EVENTLOOPTHREADPOOL_H

#include "base/noncopyable.h"
#include "EpollTaskScheduler.h"

namespace lsy::net {

/**
 * @brief 线程智能指针类型别名
 */
using ThreadPtr = std::shared_ptr<std::thread>;

/**
 * @brief 事件循环线程池
 * @details 基于EpollTaskScheduler实现的IO线程池，管理多个事件循环线程，
 *          提供负载均衡的调度器获取接口，实现网络IO的多线程并发处理
 */
class EventLoopThreadPool : noncopyable {
public:
    /**
     * @brief 构造函数
     * @param num_threads 线程池线程数量
     * @details 初始化线程池参数，创建对应数量的EpollTaskScheduler实例
     */
    explicit EventLoopThreadPool(uint32_t num_threads)
        : next_index_(num_threads > 1 ? 1 : 0),
          num_threads_(num_threads > 1 ? num_threads : 1),
          is_started_(false) {
        for (uint32_t i = 0; i < num_threads_; ++i) {
            task_schedulers_.emplace_back(new EpollTaskScheduler(i));
        }
    }

    /**
     * @brief 析构函数
     * @details 退出线程池，停止所有任务调度器并回收线程资源
     */
    virtual ~EventLoopThreadPool() {
        Quit();
    }

    /**
     * @brief 添加定时任务
     * @param event 定时器触发的回调事件
     * @param msec 定时延迟时间（毫秒）
     * @return 定时器唯一ID，用于后续删除定时器
     */
    TimerId AddTimer(const TimerEvent &event, uint32_t msec) {
        return task_schedulers_[0]->AddTimer(event, msec);
    }

    /**
     * @brief 移除定时任务
     * @param timerId 待移除的定时器唯一ID
     */
    void RemoveTimer(TimerId timerId) {
        task_schedulers_[0]->RemoveTimer(timerId);
    }

    /**
     * @brief 获取用于接收连接的任务调度器
     * @return 主任务调度器智能指针（固定使用索引0）
     */
    TaskSchedulerPtr GetAcceptTaskScheduler() {
        return task_schedulers_[0];
    }

    /**
     * @brief 轮询获取下一个IO任务调度器（负载均衡）
     * @return 下一个可用的任务调度器智能指针
     * @details 多线程环境下轮询分配调度器，实现IO请求均匀分发
     */
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

    /**
     * @brief 启动事件循环线程池
     * @details 创建并启动所有工作线程，主线程运行主调度器
     */
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
    /**
     * @brief 退出线程池，释放所有资源
     * @details 停止所有任务调度器，等待线程退出并清理资源
     */
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

    /// 线程池总线程数量
    uint32_t num_threads_;
    /// 轮询调度器索引，实现负载均衡
    uint32_t next_index_;
    /// 线程池启动状态标记
    bool is_started_;
    /// 互斥锁，保证线程池操作线程安全
    std::mutex mutex_;
    /// 任务调度器列表，每个线程对应一个调度器
    std::vector<TaskSchedulerPtr> task_schedulers_;
    /// 工作线程列表
    std::vector<ThreadPtr> threads_;
};

/**
 * @brief 事件循环线程池智能指针类型别名
 */
using EventLoopThreadPoolPtr = std::shared_ptr<EventLoopThreadPool>;

} // lsy::net

#endif // NET_EVENTLOOPTHREADPOOL_H