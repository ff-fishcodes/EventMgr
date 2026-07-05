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

    std::cout << "--- 演示3: 系统事件（通信断连）---" << std::endl;
    api.triggerSystemEvent("comm_lost", 3, true);
    std::cout << "活跃事件: " << api.getActiveEvents().size() << std::endl;
    std::cout << "事件ID: " << api.getActiveEvents()[0].id << std::endl << std::endl;

    std::cout << "--- 演示4: 系统事件（磁盘满）---" << std::endl;
    api.triggerSystemEvent("disk_full", true);
    api.triggerSystemEvent("cpu_overload", true);
    std::cout << "活跃事件: " << api.getActiveEvents().size() << std::endl << std::endl;

    std::cout << "===== 事件管理中心 演示结束 =====" << std::endl;
    return 0;
}
