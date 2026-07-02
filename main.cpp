#include <iostream>
#include "backend/event_mgr_module.h"

int main() {
    std::cout << "===== 事件管理中心 启动 =====" << std::endl << std::endl;

    // 一键启动
    EventMgrModule::init();
    ExternalAPI& api = EventMgrModule::api();

    // observe 对接
    std::cout << "--- 演示1: observe 触发报警 ---" << std::endl;
    api.triggerAlarm(1, 3, "temp_high", true);
    std::cout << "活跃事件: " << api.getActiveEvents().size() << std::endl << std::endl;

    std::cout << "--- 演示2: observe 消除报警 ---" << std::endl;
    api.triggerAlarm(1, 3, "temp_high", false);
    std::cout << "活跃事件: " << api.getActiveEvents().size() << std::endl << std::endl;

    std::cout << "===== 事件管理中心 演示结束 =====" << std::endl;
    return 0;
}
