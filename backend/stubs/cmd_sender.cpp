#include "cmd_sender.h"
#include <iostream>

void CmdSender::send(int protocolID, const std::string& target,
                     const std::string& param) {
    // 桩实现：打印到标准输出，实际项目中通过网络向下位机发送指令
    std::cout << "[CmdSender::send] protocolID=" << protocolID
              << " target=" << target
              << " param=" << param << std::endl;
}
