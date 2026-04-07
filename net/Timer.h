#ifndef NET_TIMER_H
#define NET_TIMER_H

#include <map>
#include <memory>
#include <thread>
#include <functional>
#include <unordered_map>

namespace lsy::net {

/**
 * @brief 定时器唯一标识ID, 负数表示无效ID
 */
using TimerId = int;

/**
 * @brief 定时器事件回调函数类型
 * @details 返回值：true=继续定时执行，false=执行一次后销毁
 */
using TimerEvent = std::function<bool(void)>;

/**
 * @brief 定时器类
 * @details 封装单个定时器的核心属性，包括回调函数、执行间隔、下次触发时间
 *          不直接对外使用，由 TimerQueue 统一管理调度
 */
class Timer {
public:
    /**
     * @brief 构造函数
     * @param event 定时器触发的回调函数
     * @param msec 定时器执行间隔（毫秒）
     */
    Timer(TimerEvent event, uint32_t msec)
        : event_callback_(std::move(event)), interval_(msec), next_timeout_(0) {
    }

    /**
     * @brief 线程休眠函数（静态工具方法）
     * @param msec 休眠时长（毫秒）
     */
    static void Sleep(uint32_t msec) {
        std::this_thread::sleep_for(std::chrono::milliseconds(msec));
    }

private:
    friend class TimerQueue;  // 定时器队列友元类，可访问私有成员

    /**
     * @brief 设置定时器下次触发时间点
     * @param time_point 当前基准时间戳（毫秒）
     */
    void SetNextTimeout(int64_t time_point) {
        next_timeout_ = time_point + interval_;
    }

    /**
     * @brief 获取定时器下次触发时间戳
     * @return 下次触发的毫秒时间戳
     */
    [[nodiscard]] int64_t GetNextTimeout() const {
        return next_timeout_;
    }

    TimerEvent event_callback_;    // 定时器触发回调函数
    uint32_t interval_;            // 定时器执行间隔（毫秒）
    int64_t next_timeout_;         // 下次触发的时间戳（毫秒）
};

/**
 * @brief 定时器智能指针别名
 */
using TimerPtr = std::shared_ptr<Timer>;

/**
 * @brief 定时器队列管理类
 * @details 负责管理所有定时器，支持添加/删除定时器、自动触发到期事件
 *          支持单次执行和循环重复执行两种模式
 */
class TimerQueue {
public:
    /**
     * @brief 添加一个定时器
     * @param event 定时器回调函数
     * @param msec 定时器执行间隔（毫秒）
     * @return 分配的唯一定时器ID，用于后续删除操作
     */
    TimerId AddTimer(const TimerEvent &event, uint32_t msec) {
        int64_t time_point = GetTimeNow();
        TimerId timer_id = last_timer_id_++;
        auto timer = std::make_shared<Timer>(event, msec);
        timer->SetNextTimeout(time_point);
        timers_.emplace(timer_id, timer);
        events_.emplace(
            std::piecewise_construct,
            std::forward_as_tuple(timer->GetNextTimeout(), timer_id),
            std::forward_as_tuple(timer)
        );
        return timer_id;
    }

    /**
     * @brief 根据ID删除指定定时器
     * @param timer_id 要删除的定时器唯一ID
     */
    void RemoveTimer(TimerId timer_id) {
        auto it = timers_.find(timer_id);
        if (it != timers_.end()) {
            events_.erase(
                std::make_pair(it->second->GetNextTimeout(), timer_id));
            timers_.erase(it);
        }
    }

    /**
     * @brief 处理所有到期的定时器事件
     * @details 遍历并执行已到期事件，重复执行的定时器自动重新入队
     */
    void HandleTimerEvent() {
        if (timers_.empty()) {
            return;
        }
        int64_t time_point = GetTimeNow();
        while (!timers_.empty() &&
               events_.begin()->first.first <= time_point) {
            TimerId timer_id = events_.begin()->first.second;
            bool is_repeated = events_.begin()->second->event_callback_();
            if (is_repeated) {
                TimerPtr timer = std::move(events_.begin()->second);
                timer->SetNextTimeout(time_point);
                events_.erase(events_.begin());
                events_.emplace(
                    std::piecewise_construct,
                    std::forward_as_tuple(
                        timer->GetNextTimeout(),
                        timer_id
                    ),
                    std::forward_as_tuple(timer)
                );
            } else {
                events_.erase(events_.begin());
                timers_.erase(timer_id);
            }
        }
    }

private:
    /**
     * @brief 获取当前系统时间戳（毫秒）
     * @return 从1970-01-01至今的毫秒数
     */
    static inline int64_t GetTimeNow() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
    }

    // 定时器集合: 定时器ID -> 定时器对象
    std::unordered_map<TimerId, TimerPtr> timers_;
    // 事件队列: (下次触发时间戳, 定时器ID) -> 定时器对象
    std::map<std::pair<int64_t, TimerId>, TimerPtr> events_;
    // 定时器自增ID
    TimerId last_timer_id_ = 0;
};

} // lsy::net

#endif // NET_TIMER_H
