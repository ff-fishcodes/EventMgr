#include "cmd_sender.h"
#include <iostream>
#include <thread>
#include <chrono>

void CmdSender::send(const std::string& deviceName, const std::string& target,
                     const std::string& param) {
    // 桩实现：模拟发送指令并等待 ACK（阻塞 2 秒），验证线程池异步执行
    std::cout << "[CmdSender::send] deviceName=" << deviceName
              << " target=" << target
              << " param=" << param
              << " [等待ACK...]" << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(2));
    std::cout << "[CmdSender::ack]  deviceName=" << deviceName
              << " target=" << target << " [ACK收到]" << std::endl;
}
