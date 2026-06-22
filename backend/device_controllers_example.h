#ifndef DEVICE_CONTROLLERS_EXAMPLE_H
#define DEVICE_CONTROLLERS_EXAMPLE_H
// ============================================================
// 示例：各下位机控制单例 + 按 (type, protocolID) 注册 handler
//
// 实际情况中每个下位机一个单例，由各设备团队独立维护。
// 每个单例在初始化时向 LinkageEngine 注册自己的 handler，
// 只会收到本 protocolID 的事件分发。
// ============================================================

#include "event_types.h"
#include "linkage_engine.h"
#include <iostream>
#include <map>
#include <functional>
#include <sstream>

// ============================================================
// 示例1：锅炉控制单例（protocolID = 1）
// ============================================================
class BoilerController {
public:
    static BoilerController& instance() {
        static BoilerController inst;
        return inst;
    }

    // 注册本设备的所有指令 handler
    void registerWith(LinkageEngine& engine) {
        // ========== 方式A：按 target 字符串分发 ==========
        engine.registerHandler(LinkageAction::SendCommand, deviceProtocolID_,
            [this](const Event&, const LinkageAction& a) {
                dispatch(a.target, a.param);
            });
    }

private:
    static const int deviceProtocolID_ = 1;

    // 设备内部指令分发 — 不同指令不同签名
    void dispatch(const std::string& cmd, const std::string& param) {
        if (cmd == "emergency_stop") {
            int reasonCode = param.empty() ? 0 : std::stoi(param);
            emergencyStop(reasonCode);
        }
        else if (cmd == "reduce_power") {
            // param 为 JSON: {"mw": 50.0, "duration": 30}
            // C++11 下用简单解析或固定格式，实际项目可用 jsoncpp
            float mw = 50.0f;
            int duration = 30;
            sscanf(param.c_str(), "%f,%d", &mw, &duration);
            reducePower(mw, duration);
        }
        else if (cmd == "vent_steam") {
            ventSteam();
        }
    }

    void emergencyStop(int reasonCode) {
        std::cout << "[BoilerController] 紧急停机 reason=" << reasonCode << std::endl;
    }

    void reducePower(float mw, int durationSec) {
        std::cout << "[BoilerController] 降功率至 " << mw
                  << "MW 持续 " << durationSec << "s" << std::endl;
    }

    void ventSteam() {
        std::cout << "[BoilerController] 蒸汽排放" << std::endl;
    }
};

// ============================================================
// 示例2：冷却塔控制单例（protocolID = 2）
// ============================================================
class CoolingTowerController {
public:
    static CoolingTowerController& instance() {
        static CoolingTowerController inst;
        return inst;
    }

    void registerWith(LinkageEngine& engine) {
        engine.registerHandler(LinkageAction::SendCommand, deviceProtocolID_,
            [this](const Event&, const LinkageAction& a) {
                dispatch(a.target, a.param);
            });
    }

private:
    static const int deviceProtocolID_ = 2;

    void dispatch(const std::string& cmd, const std::string& param) {
        if (cmd == "emergency_stop") {
            emergencyStop(param == "immediate" ? true : false);
        }
        else if (cmd == "set_fan_speed") {
            int rpm = param.empty() ? 0 : std::stoi(param);
            setFanSpeed(rpm);
        }
    }

    void emergencyStop(bool immediate) {
        std::cout << "[CoolingTower] 紧急停机 immediate="
                  << (immediate ? "true" : "false") << std::endl;
    }

    void setFanSpeed(int rpm) {
        std::cout << "[CoolingTower] 风扇转速 " << rpm << "RPM" << std::endl;
    }
};

// ============================================================
// 业务方使用示例
// ============================================================
inline void demoPerDeviceRegistration() {
    std::cout << "\n=== 演示：各下位机独立注册 handler ===\n" << std::endl;

    LinkageEngine engine;

    // 各设备单例在系统初始化时注册
    BoilerController::instance().registerWith(engine);
    CoolingTowerController::instance().registerWith(engine);

    // 也要注册 LockUI / UnlockUI 等通用 handler（省略，见 setup.cpp）
    // ... registerLockUIHandler(engine);
    // ... registerUnlockUIHandler(engine);

    // 构造一个锅炉紧急事件
    Event boilerAlarm;
    boilerAlarm.protocolID = 1;
    boilerAlarm.id = "1-3-temp_high";
    boilerAlarm.description = "下位机1-温度过高";
    boilerAlarm.activeActions.push_back(
        LinkageAction(LinkageAction::SendCommand, "emergency_stop", "99"));
    boilerAlarm.activeActions.push_back(
        LinkageAction(LinkageAction::SendCommand, "reduce_power", "30.0,20"));

    // 构造一个冷却塔事件
    Event towerAlarm;
    towerAlarm.protocolID = 2;
    towerAlarm.id = "2-1-vibration";
    towerAlarm.description = "下位机2-振动异常";
    towerAlarm.activeActions.push_back(
        LinkageAction(LinkageAction::SendCommand, "emergency_stop", "immediate"));
    towerAlarm.activeActions.push_back(
        LinkageAction(LinkageAction::SendCommand, "set_fan_speed", "800"));

    // 触发 → 各设备只收到自己的事件
    std::cout << "--- 触发锅炉事件 ---" << std::endl;
    engine.executeActive(boilerAlarm);

    std::cout << "\n--- 触发冷却塔事件 ---" << std::endl;
    engine.executeActive(towerAlarm);
}
// 输出:
// [BoilerController] 紧急停机 reason=99
// [BoilerController] 降功率至 30MW 持续 20s
// [CoolingTower] 紧急停机 immediate=true
// [CoolingTower] 风扇转速 800RPM

#endif
