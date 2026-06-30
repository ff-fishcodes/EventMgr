#include "action_registry.h"
#include "stubs/cmd_sender.h"
#include "stubs/buzzer_control.h"
#include <iostream>

void ActionRegistry::setup(LinkageEngine& engine) {

    // ============================================================
    // 下位机1：锅炉
    // ============================================================

    engine.registerAction(1, "emergency_stop", []() {
        std::cout << "[Boiler] 紧急停机 reason=99" << std::endl;
        CmdSender::send(1, "emergency_stop", "99");
    });

    engine.registerAction(1, "reduce_power", []() {
        std::cout << "[Boiler] 降功率至 30MW 持续 20s" << std::endl;
        CmdSender::send(1, "reduce_power", "30.0,20");
    });

    // ============================================================
    // 下位机2：冷却塔
    // ============================================================

    engine.registerAction(2, "emergency_stop", []() {
        std::cout << "[CoolingTower] 紧急停机 immediate=true" << std::endl;
        CmdSender::send(2, "emergency_stop", "immediate");
    });

    engine.registerAction(2, "fan_high", []() {
        std::cout << "[CoolingTower] 风扇转速 1200RPM" << std::endl;
        CmdSender::send(2, "set_fan_speed", "1200");
    });

    // ============================================================
    // 蜂鸣器（全局）
    // ============================================================

    engine.registerAction(0, "buzzer:alert", []() {
        BuzzerControl::play("alert", "");
    });

    // ============================================================
    // 事件联动配置（只含设备命令，UI 锁控由前端自行处理）
    // ============================================================

    // 锅炉温度过高：冷却塔提速，emergency_stop 由 setLevelDefault 自动附加
    engine.configureEvent("1-3-temp_high",
        {"2:fan_high"},
        {});

    // 冷却塔振动异常：自己停机
    engine.configureEvent("2-1-vibration",
        {"emergency_stop"},
        {});

    // 下位机4断联（无设备命令，前端收到 alarm_active 自行处理 UI）
    engine.configureEvent("4-0-device_offline",
        {},
        {});

    // ============================================================
    // 等级默认动作
    // ============================================================

    engine.setLevelDefault(EventLevel::Emergency,
        {"2:emergency_stop"});   // 任何 Emergency 事件 → 冷却塔停机
}
