#include "action_registry.h"
#include "stubs/cmd_sender.h"
#include "stubs/buzzer_control.h"
#include <iostream>

void ActionRegistry::setup(LinkageEngine& engine) {

    // ============================================================
    // 能力注册：名字 → 回调（参数已捕获在 lambda 中）
    // ============================================================

    engine.registerAction("boiler_stop", []() {
        std::cout << "[Boiler] 紧急停机 reason=99" << std::endl;
        CmdSender::send(1, "emergency_stop", "99");
    });

    engine.registerAction("boiler_reduce", []() {
        std::cout << "[Boiler] 降功率至 30MW 持续 20s" << std::endl;
        CmdSender::send(1, "reduce_power", "30.0,20");
    });

    engine.registerAction("cooler_stop", []() {
        std::cout << "[CoolingTower] 紧急停机 immediate=true" << std::endl;
        CmdSender::send(2, "emergency_stop", "immediate");
    });

    engine.registerAction("cooler_fan", []() {
        std::cout << "[CoolingTower] 风扇转速 1200RPM" << std::endl;
        CmdSender::send(2, "set_fan_speed", "1200");
    });

    engine.registerAction("buzzer_alert", []() {
        BuzzerControl::play("alert", "");
    });

    // ============================================================
    // 事件联动配置：事件 → 能力列表
    // ============================================================

    engine.configureEvent("1-3-temp_high",
        {"cooler_fan"},
        {});

    engine.configureEvent("2-1-vibration",
        {"cooler_stop"},
        {});

    engine.configureEvent("4-0-device_offline",
        {},
        {});

    // ============================================================
    // 等级默认：所有 Emergency 事件 → 冷却塔停机
    // ============================================================

    engine.setLevelDefault(EventLevel::Emergency,
        {"cooler_stop"});
}
