#ifndef REMOTE_CONTROL_TCPSOCKET_H
#define REMOTE_CONTROL_TCPSOCKET_H

#include <string>

namespace lsy::net {

/**
 * @brief Socket工具类
 * @details 提供套接字属性设置的静态工具函数，包括阻塞模式、地址复用、缓冲区大小等
 */
class SocketUtil {
public:
    /**
     * @brief 设置套接字为非阻塞模式
     * @param sockfd 待设置的套接字文件描述符
     * @return 无返回值
     */
    static void SetNonBlock(int sockfd);

    /**
     * @brief 设置套接字为阻塞模式
     * @param sockfd 待设置的套接字文件描述符
     * @return 无返回值
     */
    static void SetBlock(int sockfd);

    /**
     * @brief 设置套接字地址复用
     * @param sockfd 待设置的套接字文件描述符
     * @return 无返回值
     */
    static void SetReuseAddr(int sockfd);

    /**
     * @brief 设置套接字端口复用
     * @param sockfd 待设置的套接字文件描述符
     * @return 无返回值
     */
    static void SetReusePort(int sockfd);

    /**
     * @brief 开启TCP保活机制
     * @param sockfd 待设置的套接字文件描述符
     * @return 无返回值
     */
    static void SetKeepAlive(int sockfd);

    /**
     * @brief 设置套接字发送缓冲区大小
     * @param sockfd 待设置的套接字文件描述符
     * @param size 发送缓冲区大小（字节）
     * @return 无返回值
     */
    static void SetSendBufSize(int sockfd, int size);

    /**
     * @brief 设置套接字接收缓冲区大小
     * @param sockfd 待设置的套接字文件描述符
     * @param size 接收缓冲区大小（字节）
     * @return 无返回值
     */
    static void SetRecvBufSize(int sockfd, int size);
};

/**
 * @brief TCP套接字封装类
 * @details 封装Linux TCP套接字的创建、绑定、监听、接受连接、关闭等核心操作
 */
class TcpSocket {
public:
    /**
     * @brief 默认构造函数
     * @details 初始化套接字文件描述符为-1
     */
    TcpSocket() = default;

    /**
     * @brief 虚析构函数
     * @details 释放套接字资源
     */
    virtual ~TcpSocket() = default;

    /**
     * @brief 创建TCP套接字
     * @return 成功返回0，失败返回-1
     */
    int Create();

    /**
     * @brief 绑定IP地址和端口
     * @param ip 待绑定的IPv4地址字符串
     * @param port 待绑定的端口号
     * @return 绑定成功返回true，失败返回false
     */
    bool Bind(const std::string& ip, short port);

    /**
     * @brief 监听套接字连接请求
     * @param backlog 监听队列的最大长度
     * @return 监听成功返回true，失败返回false
     */
    bool Listen(int backlog);

    /**
     * @brief 接受客户端的TCP连接
     * @return 成功返回客户端套接字文件描述符，失败返回-1
     */
    int Accept();

    /**
     * @brief 关闭套接字
     * @return 无返回值
     */
    void Close();

    /**
     * @brief 半关闭套接字写端
     * @details 关闭发送通道，保留接收通道，实现TCP优雅断开
     * @return 无返回值
     */
    void ShutdownWrite();

    /**
     * @brief 获取当前套接字文件描述符
     * @return 套接字文件描述符
     */
    int GetSocket() const { return sockfd_; }

private:
    int sockfd_ = -1;  ///< 套接字文件描述符，初始化为-1表示无效
};

} // lsy::net

#endif //REMOTE_CONTROL_TCPSOCKET_H