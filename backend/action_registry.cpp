#include "action_registry.h"
#include "stubs/socket_server.h"
#include "stubs/cmd_sender.h"
#include "stubs/buzzer_control.h"
#include <sstream>
#include <iostream>

void ActionRegistry::setup(LinkageEngine& engine) {

    // ============================================================
    // 全局 UI 动作（protocolID=0）
    // ============================================================

    engine.registerAction(0, "lock_ui:panel_main", []() {
        std::cout << "[Action] lock_ui panel_main" << std::endl;
        SocketServer::notifyFrontend(
            "{\"type\":\"lock_ui\",\"target\":\"panel_main\"}");
    });

    engine.registerAction(0, "unlock_ui:panel_main", []() {
        std::cout << "[Action] unlock_ui panel_main" << std::endl;
        SocketServer::notifyFrontend(
            "{\"type\":\"unlock_ui\",\"target\":\"panel_main\"}");
    });

    engine.registerAction(0, "lock_ui:panel_device4", []() {
        SocketServer::notifyFrontend(
            "{\"type\":\"lock_ui\",\"target\":\"panel_device4\"}");
    });

    engine.registerAction(0, "unlock_ui:panel_device4", []() {
        SocketServer::notifyFrontend(
            "{\"type\":\"unlock_ui\",\"target\":\"panel_device4\"}");
    });

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
    // 事件联动配置
    // ============================================================

    // 锅炉温度过高（Emergency）：冷却塔提速 + 锁面板
    // emergency_stop 由 setLevelDefault(Emergency) 自动附加
    engine.configureEvent("1-3-temp_high",
        {"2:fan_high", "lock_ui:panel_main"},
        {});

    // 冷却塔振动异常（Serious）：自己停机
    engine.configureEvent("2-1-vibration",
        {"emergency_stop"},
        {});

    // 下位机4断联：锁界面
    engine.configureEvent("4-0-device_offline",
        {"lock_ui:panel_device4"},
        {});

    // ============================================================
    // 等级默认动作
    // ============================================================

    engine.setLevelDefault(EventLevel::Emergency,
        {"2:emergency_stop"});   // 任何 Emergency 事件 → 冷却塔停机
}
