#ifndef NET_BUFFERWRITER_H
#define NET_BUFFERWRITER_H

#include <sys/socket.h>
#include <cerrno>

#include <queue>
#include <memory>
#include <cstring>

#include "base/noncopyable.h"

namespace lsy::net {

/**
 * @brief 非阻塞Socket发送缓冲区类
 * @details 用于缓存待发送的网络数据包，支持非阻塞模式下批量发送数据
 *          自动处理数据包部分发送、Socket缓冲区满、多包连续发送等场景
 */
class BufferWriter {
public:
    /**
     * @brief 构造函数
     * @param capacity 缓冲区最大队列长度，默认使用类内常量kMaxQueueLength
     */
    explicit BufferWriter(size_t capacity = kMaxQueueLength)
        : max_queue_length_(capacity) {
    };

    /**
     * @brief 析构函数
     */
    ~BufferWriter() = default;

    /**
     * @brief 添加共享指针类型的数据包到发送缓冲区
     * @param data 数据包共享指针
     * @param size 数据包总长度
     * @param index 数据发送起始偏移量，默认为0
     * @return  添加成功返回true，缓冲区满/参数非法返回false
     */
    bool Append(const std::shared_ptr<char[]> &data, uint32_t size,
                uint32_t index = 0) {
        if (size == 0 || size < index || !data) {
            return false;
        }
        if (buffer_.size() >= max_queue_length_) {
            return false;
        }
        buffer_.emplace(data, size, index);
        return true;
    }

    /**
     * @brief 添加普通字符指针类型的数据包到发送缓冲区(会多一次数据拷贝)
     * @param data 原始数据指针
     * @param size 数据包总长度
     * @param index 数据发送起始偏移量，默认为0
     * @return 添加成功返回true，缓冲区满/参数非法返回false
     */
    bool Append(const char *data, uint32_t size, uint32_t index = 0) {
        if (size == 0 || size < index || !data) {
            return false;
        }
        if (buffer_.size() >= max_queue_length_) {
            return false;
        }
        std::shared_ptr<char[]> data_ptr(new char[size]);
        std::memcpy(data_ptr.get(), data, size);
        buffer_.emplace(data_ptr, size, index);
        return true;
    }

    /**
     * @brief 非阻塞发送缓冲区中的所有数据包
     * @details 循环发送队首数据包，处理部分发送/缓冲区满/发送完成场景
     * @param sockfd 用于发送数据的套接字文件描述符
     * @return 1=所有数据发送完成，0=Socket缓冲区满暂无法发送，-1=发送发生错误
     */
    ssize_t Send(int sockfd) {
        if (buffer_.empty()) {
            return 0;
        }

        ssize_t ret;
        while (true) {
            Packet &packet = buffer_.front();
            ret = ::send(sockfd, packet.data.get() + packet.write_index,
                         packet.size - packet.write_index, 0);
            if (ret > 0) {
                packet.write_index += ret;
                if (packet.write_index == packet.size) {
                    buffer_.pop();
                    if (buffer_.empty()) {
                        return 1; // 成功发完所有数据
                    }
                }
                continue;
            } else if (ret == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                return 0; // socket 缓冲区满
            } else {
                return ret; // 发送失败
            }
        }
    }

    /**
     * @brief 判断发送缓冲区是否为空
     * @return 为空返回true，否则返回false
     */
    [[nodiscard]] bool Empty() const {
        return buffer_.empty();
    }

    /**
     * @brief 判断发送缓冲区是否已满
     * @return 已满返回true，否则返回false
     */
    [[nodiscard]] bool Full() const {
        return buffer_.size() >= max_queue_length_;
    }

    /**
     * @brief 获取发送缓冲区中待发送的数据包数量
     * @return 数据包个数
     */
    [[nodiscard]] size_t Size() const {
        return buffer_.size();
    }

private:
    /**
     * @brief 数据包结构体
     * @details 存储待发送数据、数据长度、已发送偏移量
     */
    struct Packet : noncopyable {
        std::shared_ptr<char[]> data;   // 数据存储指针
        uint32_t size;                  // 数据包总大小
        uint32_t write_index;           // 已发送数据的偏移量

        Packet(const std::shared_ptr<char[]> &data, uint32_t size,
               uint32_t index)
            : data(data), size(size), write_index(index) {
        }

        Packet(Packet &&other) noexcept
            : data(std::move(other.data)),
              size(other.size),
              write_index(other.write_index) {
            other.size = 0;
            other.write_index = 0;
        }

        Packet &operator=(Packet &&other) noexcept {
            if (this != &other) {
                data = std::move(other.data);
                size = other.size;
                write_index = other.write_index;
                other.size = 0;
                other.write_index = 0;
            }
            return *this;
        }
    };

    std::queue<Packet> buffer_;                     // 数据包发送队列
    size_t max_queue_length_;                       // 队列最大长度限制
    static constexpr size_t kMaxQueueLength = 8192; // 默认最大队列长度
};

} // lsy::net

#endif // NET_BUFFERWRITER_H
