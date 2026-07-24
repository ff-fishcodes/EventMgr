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
    // 报警定义登记（设备事件 + 系统事件，外部可配置）
    // ============================================================

    // -- 设备事件 --
    api.registerAlarmDef("锅炉",   3, "temp_high",     "锅炉-温度过高", EventLevel::Emergency);
    api.registerAlarmDef("锅炉",   3, "pressure_low",  "锅炉-压力过低", EventLevel::Serious);
    api.registerAlarmDef("锅炉",   5, "overload",      "锅炉-过载",     EventLevel::Emergency);
    api.registerAlarmDef("冷却塔", 1, "vibration",     "冷却塔-振动异常", EventLevel::Serious);
    api.registerAlarmDef("冷却塔", 1, "water_level",   "冷却塔-水位过低", EventLevel::General);
    api.registerAlarmDef("电网",   2, "voltage_high",  "电网-电压过高", EventLevel::Emergency);
    api.registerAlarmDef("电网",   2, "current_high",  "电网-电流过高", EventLevel::Serious);
    api.registerAlarmDef("水泵",   0, "device_offline", "水泵-通信中断", EventLevel::Serious);

    // -- 系统事件 --
    api.registerAlarmDef("系统", 0, "comm_lost",    "通信断连", EventLevel::Emergency, true);
    api.registerAlarmDef("系统", 0, "comm_restore", "通信恢复", EventLevel::Info, true);
    api.registerAlarmDef("系统", 0, "disk_full",    "磁盘空间不足", EventLevel::Serious, true);
    api.registerAlarmDef("系统", 0, "cpu_overload", "CPU 过载", EventLevel::Serious, true);
}
