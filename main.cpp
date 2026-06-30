#include <iostream>
#include "backend/event_types.h"
#include "backend/config_manager.h"
#include "backend/linkage_engine.h"
#include "backend/event_manager.h"
#include "backend/external_api.h"
#include "backend/action_registry.h"

int main() {
    std::cout << "===== 事件管理中心 启动 =====" << std::endl << std::endl;

    // 1. 创建模块
    ConfigManager configMgr;
    LinkageEngine linkageEng;

    // 集中注册所有能力 + 配置事件联动 + 等级默认
    ActionRegistry::setup(linkageEng);

    EventManager eventMgr(configMgr, linkageEng);
    ExternalAPI api(eventMgr, configMgr);

    // 2. 运行时：业务方只创建 Event，不关心联动
    std::cout << "--- 演示1: 锅炉温度过高（跨设备联动）---" << std::endl;
    Event e1 = api.createAlarm(1, 3, "temp_high", EventLevel::Emergency, "下位机1-温度过高");
    api.addEvent(e1);
    std::cout << "活跃事件: " << eventMgr.activeCount() << std::endl << std::endl;

    std::cout << "--- 演示2: 冷却塔振动异常 ---" << std::endl;
    Event e2 = api.createAlarm(2, 1, "vibration", EventLevel::Serious, "下位机2-振动异常");
    api.addEvent(e2);
    std::cout << "活跃事件: " << eventMgr.activeCount() << std::endl << std::endl;

    std::cout << "--- 演示3: 下位机4断联 ---" << std::endl;
    Event e3 = api.createAlarm(4, 0, "device_offline", EventLevel::Serious, "下位机4-通信中断");
    api.addEvent(e3);
    std::cout << "活跃事件: " << eventMgr.activeCount() << std::endl << std::endl;

    std::cout << "--- 演示4: 消除锅炉事件 ---" << std::endl;
    api.clearEvent(1, 3, "temp_high");
    std::cout << "活跃事件: " << eventMgr.activeCount() << std::endl << std::endl;

    std::cout << "===== 事件管理中心 演示结束 =====" << std::endl;
    return 0;
}
