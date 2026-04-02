#ifndef NET_CHANNEL_H
#define NET_CHANNEL_H

#include <memory>
#include <functional>

namespace lsy::net {

/**
 * @brief IO事件枚举
 * @details 封装Linux epoll网络IO事件，支持位运算组合
 */
enum Event : uint32_t {
    EVENT_NONE = 0,
    EVENT_IN = 1,
    EVENT_PRI = 2,
    EVENT_OUT = 4,
    EVENT_ERR = 8,
    EVENT_HUP = 16,
};

/**
 * @brief 事件掩码类型
 * @details 封装Linux epoll网络IO事件，支持位运算组合
 */
using Events = uint32_t;

/**
 * @brief 文件描述符类型
 * @details 封装Linux文件描述符
 */
using FileDescriptor = int;

/**
 * @brief 通道类
 * @details 封装文件描述符、IO事件、事件回调，是Reactor模型的核心组件
 *          负责管理单个fd的事件监听、回调绑定与事件分发
 */
class Channel {
public:
    using EventCallback = std::function<void()>;

    /**
     * @brief 构造函数
     * @param fd 绑定的文件描述符
     */
    explicit Channel(FileDescriptor fd) : fd_(fd) {
    }

    /**
     * @brief 析构函数
     */
    ~Channel() = default;

    /**
     * @brief 设置读事件回调函数
     * @param cb 读事件触发时执行的回调
     */
    inline void SetReadCallback(const EventCallback &cb) {
        read_callback_ = cb;
    }

    /**
     * @brief 设置写事件回调函数
     * @param cb 写事件触发时执行的回调
     */
    inline void SetWriteCallback(const EventCallback &cb) {
        write_callback_ = cb;
    }

    /**
     * @brief 设置关闭事件回调函数
     * @param cb 连接关闭时执行的回调
     */
    inline void SetCloseCallback(const EventCallback &cb) {
        close_callback_ = cb;
    }

    /**
     * @brief 设置错误事件回调函数
     * @param cb 发生错误时执行的回调
     */
    inline void SetErrorCallback(const EventCallback &cb) {
        error_callback_ = cb;
    }

    /**
     * @brief 获取绑定的文件描述符
     * @return fd
     */
    inline FileDescriptor GetFd() const { return fd_; }

    /**
     * @brief 获取当前监听的事件掩码
     * @return 事件掩码值
     */
    inline Events GetEvents() const { return events_; }

    /**
     * @brief 设置监听的事件掩码
     * @param events 待设置的事件掩码
     */
    inline void SetEvents(int events) { events_ = events; }

    /**
     * @brief 开启读事件监听
     */
    inline void EnableReading() { events_ |= EVENT_IN; }

    /**
     * @brief 开启写事件监听
     */
    inline void EnableWriting() { events_ |= EVENT_OUT; }

    /**
     * @brief 关闭读事件监听
     */
    inline void DisableReading() { events_ &= ~EVENT_IN; }

    /**
     * @brief 关闭写事件监听
     */
    inline void DisableWriting() { events_ &= ~EVENT_OUT; }

    /**
     * @brief 判断是否未监听任何事件
     * @return 无事件返回true，否则返回false
     */
    inline bool IsNoneEvent() const { return events_ == EVENT_NONE; }

    /**
     * @brief 判断是否开启写事件监听
     * @return 开启写事件返回true，否则返回false
     */
    inline bool IsWriting() const { return (events_ & EVENT_OUT) != 0; }

    /**
     * @brief 判断是否开启读事件监听
     * @return 开启读事件返回true，否则返回false
     */
    inline bool IsReading() const { return (events_ & EVENT_IN) != 0; }

    /**
     * @brief 事件分发处理函数
     * @param events 触发的事件类型
     * @details 根据触发的IO事件，调用对应的回调函数
     */
    void HandleEvent(Events events) {
        if (events & (EVENT_PRI | EVENT_IN)) {
            read_callback_();
        }
        if (events & EVENT_OUT) {
            write_callback_();
        }
        if (events & EVENT_HUP) {
            close_callback_();
            return;
        }
        if (events & (EVENT_ERR)) {
            error_callback_();
        }
    }

private:
    EventCallback read_callback_ = []() -> void {};
    EventCallback write_callback_ = []() -> void {};
    EventCallback close_callback_ = []() -> void {};
    EventCallback error_callback_ = []() -> void {};
    FileDescriptor fd_ = 0;
    Events events_ = 0;
};

/**
 * @brief 通道智能指针别名
 * @details 简化std::shared_ptr<Channel>的使用
 */
using ChannelPtr = std::shared_ptr<Channel>;

} // lsy::net

#endif // NET_CHANNEL_H
