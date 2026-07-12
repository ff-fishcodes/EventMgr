#include <iostream>
#include <thread>
#include <chrono>
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

    std::cout << "--- 演示5: 未注册系统事件 + 设备事件不在目录 ---" << std::endl;
    api.triggerSystemEvent("unknown_event", true);
    api.triggerAlarm(9, 9, "unknown_field", true);
    std::cout << "活跃事件: " << api.getActiveEvents().size() << std::endl << std::endl;

    std::cout << "--- 演示6: 线程池并发联动 ---" << std::endl;
    std::cout << "同时触发3个有联动的紧急事件（每个等ACK 2秒）..." << std::endl;
    api.triggerAlarm(1, 3, "temp_high", true);      // Emergency → cooler_fan + cooler_stop
    api.triggerAlarm(2, 1, "vibration", true);       // Serious → cooler_stop
    api.triggerAlarm(3, 2, "voltage_high", true);    // Emergency → cooler_stop (level default)
    std::cout << "3个事件已提交（瞬间返回），联动在4线程池中并发执行..."
              << std::endl;
    std::cout << "等待联动完成..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(5));
    std::cout << "活跃事件: " << api.getActiveEvents().size() << std::endl << std::endl;

    std::cout << "===== 事件管理中心 演示结束 =====" << std::endl;
    return 0;
}
