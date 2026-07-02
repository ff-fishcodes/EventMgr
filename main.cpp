#include <iostream>
#include "backend/event_types.h"
#include "backend/config_manager.h"
#include "backend/linkage_engine.h"
#include "backend/event_manager.h"
#include "backend/external_api.h"
#include "backend/action_registry.h"

int main() {
    std::cout << "===== 事件管理中心 启动 =====" << std::endl << std::endl;

    ConfigManager configMgr;
    LinkageEngine linkageEng;
    ActionRegistry::setup(linkageEng);

    EventManager eventMgr(configMgr, linkageEng);
    ExternalAPI api(eventMgr, configMgr);

    // ---- observe 组件对接：只需传三个参数，等级+描述自动查 ----
    std::cout << "--- 演示1: observe 触发报警 ---" << std::endl;
    api.triggerAlarm(1, 3, "temp_high", true);
    std::cout << "活跃事件: " << eventMgr.activeCount() << std::endl << std::endl;

    std::cout << "--- 演示2: observe 消除报警 ---" << std::endl;
    api.triggerAlarm(1, 3, "temp_high", false);
    std::cout << "活跃事件: " << eventMgr.activeCount() << std::endl << std::endl;

    // ---- 手动模式：指定等级+描述 ----
    std::cout << "--- 演示3: 冷却塔振动异常 ---" << std::endl;
    Event e = api.createAlarm(2, 1, "vibration", EventLevel::Serious, "下位机2-振动异常");
    api.addEvent(e);
    std::cout << "活跃事件: " << eventMgr.activeCount() << std::endl << std::endl;

    std::cout << "--- 演示4: 重复上报抑制 ---" << std::endl;
    api.addEvent(e);
    std::cout << "活跃事件: " << eventMgr.activeCount() << " (不变)" << std::endl << std::endl;

    std::cout << "--- 演示5: 消除 ---" << std::endl;
    api.clearEvent(2, 1, "vibration");
    std::cout << "活跃事件: " << eventMgr.activeCount() << std::endl << std::endl;

    std::cout << "===== 事件管理中心 演示结束 =====" << std::endl;
    return 0;
}
