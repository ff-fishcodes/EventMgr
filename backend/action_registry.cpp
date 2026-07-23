#include "action_registry.h"
#include "external_api.h"
#include "stubs/cmd_sender.h"
#include "stubs/buzzer_control.h"
#include <iostream>

void ActionRegistry::setup() {
    ExternalAPI& api = ExternalAPI::instance();

    // ============================================================
    // 能力注册：名字 → 回调（参数已捕获在 lambda 中）
    // ============================================================

    api.registerAction("boiler_stop", "停锅炉", []() {
        std::cout << "[Boiler] 紧急停机 reason=99" << std::endl;
        CmdSender::send("锅炉", "emergency_stop", "99");
    });

    api.registerAction("boiler_reduce", "降功率", []() {
        std::cout << "[Boiler] 降功率至 30MW 持续 20s" << std::endl;
        CmdSender::send("锅炉", "reduce_power", "30.0,20");
    });

    api.registerAction("cooler_stop", "关冷却塔", []() {
        std::cout << "[CoolingTower] 紧急停机 immediate=true" << std::endl;
        CmdSender::send("冷却塔", "emergency_stop", "immediate");
    });

    api.registerAction("cooler_fan", "调风扇", []() {
        std::cout << "[CoolingTower] 风扇转速 1200RPM" << std::endl;
        CmdSender::send("冷却塔", "set_fan_speed", "1200");
    });

    api.registerAction("buzzer_alert", "蜂鸣器", []() {
        BuzzerControl::play("alert", "");
    });

    // ============================================================
    // 事件联动配置：事件 → 能力列表
    // ============================================================

    api.configureEvent("锅炉-3-temp_high", {"cooler_fan"}, {});
    api.configureEvent("冷却塔-1-vibration", {"cooler_stop"}, {});
    api.configureEvent("水泵-0-device_offline", {}, {});

    // 等级默认：所有 Emergency 事件产生阶段停冷却塔，消除阶段无默认动作
    api.setLevelDefault(EventLevel::Emergency,
                        {"cooler_stop"}, std::vector<std::string>());

    // ============================================================
    // 系统事件定义登记（外部可配置）
    // ============================================================

    api.addSystemEventDef("comm_lost",    "通信断连", EventLevel::Emergency);
    api.addSystemEventDef("comm_restore", "通信恢复", EventLevel::Info);
    api.addSystemEventDef("disk_full",    "磁盘空间不足", EventLevel::Serious);
    api.addSystemEventDef("cpu_overload", "CPU 过载", EventLevel::Serious);
}
