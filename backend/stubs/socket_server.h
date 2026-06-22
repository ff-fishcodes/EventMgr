#ifndef SOCKET_SERVER_H
#define SOCKET_SERVER_H

#include <string>

// ============================================================
// 桩模块：Socket 服务端 — 前后端通信
// 属项目已有基础设施，本需求不实现，仅提供接口声明
// ============================================================
class SocketServer {
public:
    // 向前端推送 JSON 格式事件变更通知
    // jsonMessage 格式见设计文档 5.3 节
    static void notifyFrontend(const std::string& jsonMessage);
};

#endif // SOCKET_SERVER_H
