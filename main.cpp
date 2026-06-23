#include <iostream>
#include "backend/event_types.h"
#include "backend/config_manager.h"
#include "backend/linkage_engine.h"
#include "backend/event_manager.h"
#include "backend/external_api.h"
#include "backend/setup.h"
#include "backend/device_controllers_example.h"

int main() {
    std::cout << "===== 事件管理中心 启动 =====" << std::endl << std::endl;

    // ============================================================
    // 1. 启动阶段：初始化 + 注册 handler + 配置事件联动
    // ============================================================

    ConfigManager configMgr;
    LinkageEngine linkageEng;

    // 注册通用 UI handler（全局）
    registerLinkageHandlers(linkageEng);

    // 各设备控制单例注册"我能干什么"
    BoilerController::instance().registerWith(linkageEng);
    CoolingTowerController::instance().registerWith(linkageEng);

    // ---- 事件联动配置（启动时一次性完成）----

    // 锅炉温度过高：自停 + 冷却塔提速散热（跨设备联动）
    {
        std::vector<LinkageAction> active, clear;
        active.push_back(LinkageAction(LinkageAction::SendCommand, "emergency_stop", "99", 1));
        active.push_back(LinkageAction(LinkageAction::SendCommand, "set_fan_speed", "1200", 2));
        active.push_back(LinkageAction(LinkageAction::LockUI, "panel_main", "", 0));
        clear.push_back(LinkageAction(LinkageAction::UnlockUI, "panel_main", "", 0));
        linkageEng.configureEvent("1-3-temp_high", active, clear);
    }

    // 冷却塔振动异常：只停自己
    {
        std::vector<LinkageAction> active, clear;
        active.push_back(LinkageAction(LinkageAction::SendCommand, "emergency_stop", "immediate", 2));
        linkageEng.configureEvent("2-1-vibration", active, clear);
    }

    // 下位机4断联：只锁界面
    {
        std::vector<LinkageAction> active, clear;
        active.push_back(LinkageAction(LinkageAction::LockUI, "panel_device4", "", 0));
        clear.push_back(LinkageAction(LinkageAction::UnlockUI, "panel_device4", "", 0));
        linkageEng.configureEvent("4-0-device_offline", active, clear);
    }

    EventManager eventMgr(configMgr, linkageEng);
    ExternalAPI api(eventMgr);

    // ============================================================
    // 2. 运行阶段：业务方只创建 Event，不关心联动
    // ============================================================

    // 演示1：锅炉温度高 → 锅炉停机 + 冷却塔提速（跨设备）
    std::cout << "--- 演示1: 锅炉温度过高（跨设备联动）---" << std::endl;
    Event e1 = api.createAlarm(1, 3, "temp_high", EventLevel::Emergency, "下位机1-温度过高");
    // Event 上不需要 push 任何 action，configureEvent 已配好
    api.addEvent(e1);
    std::cout << "活跃事件: " << eventMgr.activeCount() << std::endl << std::endl;

    // 演示2：冷却塔振动 → 只停冷却塔自己
    std::cout << "--- 演示2: 冷却塔振动异常 ---" << std::endl;
    Event e2 = api.createAlarm(2, 1, "vibration", EventLevel::Serious, "下位机2-振动异常");
    api.addEvent(e2);
    std::cout << "活跃事件: " << eventMgr.activeCount() << std::endl << std::endl;

    // 演示3：断联事件
    std::cout << "--- 演示3: 下位机4断联 ---" << std::endl;
    Event e3 = api.createAlarm(4, 0, "device_offline", EventLevel::Serious, "下位机4-通信中断");
    api.addEvent(e3);
    std::cout << "活跃事件: " << eventMgr.activeCount() << std::endl << std::endl;

    // 演示4：消除锅炉事件 → 解锁 UI
    std::cout << "--- 演示4: 消除锅炉事件 ---" << std::endl;
    api.clearEvent(1, 3, "temp_high");
    std::cout << "活跃事件: " << eventMgr.activeCount() << std::endl << std::endl;

    std::cout << "===== 事件管理中心 演示结束 =====" << std::endl;
    return 0;
}
