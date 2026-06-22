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

    // 1. 初始化各模块
    ConfigManager configMgr;
    LinkageEngine linkageEng;

    // 注册通用联动 handler（LockUI, UnlockUI, Buzzer）
    registerLinkageHandlers(linkageEng);

    // 各下位机控制单例注册自己的指令 handler
    BoilerController::instance().registerWith(linkageEng);
    CoolingTowerController::instance().registerWith(linkageEng);

    // 组装
    EventManager eventMgr(configMgr, linkageEng);
    ExternalAPI api(eventMgr);

    // 2. 演示：锅炉紧急告警 — SendCommand 分发到 BoilerController
    std::cout << "--- 演示1: 锅炉紧急告警（SendCommand → BoilerController）---" << std::endl;
    Event alarm = api.createAlarm(1, 3, "temp_high",
                                   EventLevel::Emergency, "下位机1-温度过高");
    alarm.activeActions.push_back(
        LinkageAction(LinkageAction::SendCommand, "emergency_stop", "99"));
    alarm.activeActions.push_back(
        LinkageAction(LinkageAction::SendCommand, "reduce_power", "30.0,20"));
    alarm.activeActions.push_back(
        LinkageAction(LinkageAction::LockUI, "panel_main", ""));
    alarm.clearActions.push_back(
        LinkageAction(LinkageAction::UnlockUI, "panel_main", ""));
    api.addEvent(alarm);
    std::cout << "活跃事件: " << eventMgr.activeCount() << std::endl << std::endl;

    // 3. 演示：冷却塔告警 — SendCommand 只分发到 CoolingTowerController
    std::cout << "--- 演示2: 冷却塔告警（SendCommand → CoolingTowerController）---" << std::endl;
    Event alarm2 = api.createAlarm(2, 1, "vibration",
                                    EventLevel::Serious, "下位机2-振动异常");
    alarm2.activeActions.push_back(
        LinkageAction(LinkageAction::SendCommand, "emergency_stop", "immediate"));
    alarm2.activeActions.push_back(
        LinkageAction(LinkageAction::SendCommand, "set_fan_speed", "800"));
    api.addEvent(alarm2);
    std::cout << "活跃事件: " << eventMgr.activeCount() << std::endl << std::endl;

    // 4. 演示：验证锅炉事件不会触发冷却塔 controller
    //   （观察输出 — alarm2 的 set_fan_speed 不会在 BoilerController 中执行）

    // 5. 消除锅炉事件
    std::cout << "--- 演示3: 消除锅炉事件 ---" << std::endl;
    api.clearEvent(1, 3, "temp_high");
    std::cout << "活跃事件: " << eventMgr.activeCount() << std::endl << std::endl;

    std::cout << "===== 事件管理中心 演示结束 =====" << std::endl;
    return 0;
}
