#include "socket_server.h"
#include <iostream>

void SocketServer::notifyFrontend(const std::string& jsonMessage) {
    // 桩实现：打印到标准输出，实际项目中推送到前端 Qt 进程
    std::cout << "[SocketServer::notifyFrontend] " << jsonMessage << std::endl;
}
