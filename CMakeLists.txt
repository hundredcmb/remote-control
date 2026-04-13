cmake_minimum_required(VERSION 3.20)
project(remote-control)

if (NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "Only support 'Linux' system, current system is '${CMAKE_SYSTEM_NAME}'")
endif ()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(TARGET_NAME ${PROJECT_NAME})

add_executable(${TARGET_NAME}
        main.cpp
        examples/EchoServer.h
        base/noncopyable.h
        base/ByteIO.h
        net/TcpSocket.h
        net/TcpSocket.cpp
        net/Channel.h
        net/BufferReader.h
        net/BufferWriter.h
        net/Timer.h
        net/TaskScheduler.h
        net/EpollTaskScheduler.h
        net/EventLoopThreadPool.h
        net/TcpConnection.h
        net/Acceptor.h
        net/TcpServer.h
        rtmp/RtmpHandshake.h
        rtmp/RtmpMessage.h
        rtmp/RtmpMessageCodec.h
        rtmp/amf.h
        rtmp/amf.cpp
        rtmp/RtmpSink.h
        rtmp/RtmpSession.h
        rtmp/RtmpConnection.h
        rtmp/RtmpConnection.cpp
)

target_include_directories(${TARGET_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(${TARGET_NAME} PRIVATE pthread)
