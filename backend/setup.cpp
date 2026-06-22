#include "setup.h"
#include "linkage_engine.h"
#include "stubs/socket_server.h"
#include "stubs/buzzer_control.h"
#include <sstream>

void registerLinkageHandlers(LinkageEngine& engine) {

    // ============================================================
    // LockUI — 全局 handler（所有 protocolID 的事件都匹配）
    // 通过 SocketServer 通知前端执行 lock_ui
    // action.target = 前端控件语义标识（如 "panel_operation"）
    // ============================================================
    engine.registerHandler(LinkageAction::LockUI,
        [](const Event& e, const LinkageAction& a) {
            std::ostringstream json;
            json << "{"
                 << "\"type\":\"lock_ui\","
                 << "\"target\":\"" << a.target << "\","
                 << "\"eventId\":\"" << e.id << "\","
                 << "\"description\":\"" << e.description << "\""
                 << "}";
            SocketServer::notifyFrontend(json.str());
        });

    // ============================================================
    // UnlockUI — 全局 handler（所有 protocolID 的事件都匹配）
    // ============================================================
    engine.registerHandler(LinkageAction::UnlockUI,
        [](const Event& e, const LinkageAction& a) {
            std::ostringstream json;
            json << "{"
                 << "\"type\":\"unlock_ui\","
                 << "\"target\":\"" << a.target << "\","
                 << "\"eventId\":\"" << e.id << "\""
                 << "}";
            SocketServer::notifyFrontend(json.str());
        });

    // ============================================================
    // SendCommand — 各下位机控制单例自行按 protocolID 注册
    //
    // 不在此处注册通用 handler，因为不同下位机的控制指令
    // 由各自的单例组件负责，参数类型和数量各不相同。
    //
    // 各设备在初始化时调用：
    //   engine.registerHandler(SendCommand, <myProtocolID>, handler);
    //
    // 参见 backend/device_controllers_example.h
    // ============================================================

    // ============================================================
    // Buzzer — 全局 handler（预留）
    // ============================================================
    engine.registerHandler(LinkageAction::Buzzer,
        [](const Event& e, const LinkageAction& a) {
            (void)e;
            BuzzerControl::play(a.target, a.param);
        });
}
