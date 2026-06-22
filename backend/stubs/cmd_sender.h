#ifndef CMD_SENDER_H
#define CMD_SENDER_H

#include <string>

// ============================================================
// 桩模块：CmdSender — 向下位机发送管控指令
// 属项目已有基础设施，本需求不实现，仅提供接口声明
// ============================================================
class CmdSender {
public:
    // 向指定下位机发送管控指令
    // protocolID: 目标下位机
    // target:     指令名称
    // param:      指令参数
    static void send(int protocolID, const std::string& target,
                     const std::string& param);
};

#endif // CMD_SENDER_H
