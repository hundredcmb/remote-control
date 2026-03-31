#ifndef REMOTE_CONTROL_BUFFERREADER_H
#define REMOTE_CONTROL_BUFFERREADER_H

#include <sys/socket.h>

#include <vector>
#include <string>
#include <cassert>

namespace lsy::net {

/**
 * @brief Socket接收缓冲区类
 * @details 用于非阻塞模式下接收Socket数据，自动管理读写索引、缓冲区扩容
 *          提供数据读取、检索、清空等核心操作，适配高并发网络接收场景
 */
class BufferReader {
public:
    /**
     * @brief 构造函数
     * @param initial_size 缓冲区初始大小，默认4096字节
     */
    explicit BufferReader(uint32_t initial_size = 4096) {
        buffer_.resize(initial_size);
    }

    /**
     * @brief 析构函数
     */
    virtual ~BufferReader() = default;

    /**
     * @brief 获取缓冲区可读字节数
     * @return 未读取的有效数据长度
     */
    uint32_t ReadableBytes() const {
        return (uint32_t) (writer_index_ - reader_index_);
    }

    /**
     * @brief 获取缓冲区可写字节数
     * @return 剩余可用的写入空间大小
     */
    uint32_t WritableBytes() const {
        return (uint32_t) (buffer_.size() - writer_index_);
    }

    /**
     * @brief 获取可读数据的起始指针（非const）
     * @return 指向可读数据首地址的指针
     */
    char *Peek() { return Begin() + reader_index_; }

    /**
     * @brief 获取可读数据的起始指针（const）
     * @return 指向可读数据首地址的常量指针
     */
    const char *Peek() const { return Begin() + reader_index_; }

    /**
     * @brief 清空整个缓冲区
     * @details 重置读写索引为0，不释放底层内存
     */
    void RetrieveAll() {
        writer_index_ = 0;
        reader_index_ = 0;
    }

    /**
     * @brief 回收指定长度的已读数据
     * @param len 需要回收的数据长度
     * @details 超过可读长度则直接清空整个缓冲区
     */
    void Retrieve(size_t len) {
        if (len <= ReadableBytes()) {
            reader_index_ += len;
            if (reader_index_ == writer_index_) {
                RetrieveAll();
            }
        } else {
            RetrieveAll();
        }
    }

    /**
     * @brief 读取指定长度数据并转换为字符串
     * @param len 待读取的数据长度
     * @return 读取到的字符串
     */
    std::string RetrieveAsString(size_t len) {
        assert(len <= ReadableBytes());
        std::string str(Peek(), len);
        Retrieve(len);
        return str;
    }

    /**
     * @brief 读取所有可读数据并转换为字符串
     * @return 所有有效数据拼接的字符串
     */
    std::string RetrieveAllAsString() {
        return RetrieveAsString(ReadableBytes());
    }

    /**
     * @brief 从Socket文件描述符读取数据
     * @details 自动扩容缓冲区，将数据写入缓冲区尾部
     * @param sockfd 待读取的套接字文件描述符
     * @return 成功读取的字节数，-1表示读取错误，0表示连接关闭
     */
    int64_t ReadFd(int sockfd) {
        uint32_t size = WritableBytes();
        if (size < kMaxBytesPerRead) {
            uint32_t buffer_size = buffer_.size();
            if (buffer_size > kMaxBufferSize) {
                return 0;
            }
            buffer_.resize(buffer_size + kMaxBytesPerRead);
        }
        ssize_t bytes_read = ::recv(sockfd, BeginWrite(), kMaxBytesPerRead,
                                    0);
        if (bytes_read > 0) {
            writer_index_ += bytes_read;
        }
        return bytes_read;
    }

    /**
     * @brief 获取缓冲区总容量
     * @return 缓冲区底层vector的总大小
     */
    uint32_t Size() const { return (uint32_t) buffer_.size(); }

private:
    /**
     * @brief 获取缓冲区起始指针（非const）
     * @return 缓冲区内存首地址
     */
    char *Begin() { return &*buffer_.begin(); }

    /**
     * @brief 获取缓冲区起始指针（const）
     * @return 缓冲区内存首地址
     */
    const char *Begin() const { return &*buffer_.begin(); }

    /**
     * @brief 获取可写位置指针（非const）
     * @return 缓冲区可写入数据的首地址
     */
    char *BeginWrite() { return Begin() + writer_index_; }

    /**
     * @brief 获取可写位置指针（const）
     * @return 缓冲区可写入数据的首地址
     */
    const char *BeginWrite() const { return Begin() + writer_index_; }

    std::vector<char> buffer_;        // 底层数据存储容器
    size_t reader_index_ = 0;         // 读索引（下一个待读取的位置）
    size_t writer_index_ = 0;         // 写索引（下一个待写入的位置）
    static constexpr uint32_t kMaxBytesPerRead = 4096;    // 单次最大读取字节数
    static constexpr uint32_t kMaxBufferSize = 4096 * 4096; // 缓冲区最大容量限制
};

} // lsy::net

#endif //REMOTE_CONTROL_BUFFERREADER_H
