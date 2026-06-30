#ifndef DEVICE_CONTROLLERS_EXAMPLE_H
#define DEVICE_CONTROLLERS_EXAMPLE_H
// ============================================================
// 示例：各下位机控制器（仅暴露 public 方法，不负责注册）
// 注册逻辑已移至 ActionRegistry::setup()
// ============================================================

#include <iostream>

// ============================================================
// 示例1：锅炉控制器（protocolID = 1）
// ============================================================
class BoilerController {
public:
    static BoilerController& instance() {
        static BoilerController inst;
        return inst;
    }

    void emergencyStop(int reasonCode) {
        std::cout << "[BoilerController] 紧急停机 reason=" << reasonCode << std::endl;
    }

    void reducePower(float mw, int durationSec) {
        std::cout << "[BoilerController] 降功率至 " << mw
                  << "MW 持续 " << durationSec << "s" << std::endl;
    }
};

// ============================================================
// 示例2：冷却塔控制器（protocolID = 2）
// ============================================================
class CoolingTowerController {
public:
    static CoolingTowerController& instance() {
        static CoolingTowerController inst;
        return inst;
    }

    void emergencyStop(bool immediate) {
        std::cout << "[CoolingTower] 紧急停机 immediate="
                  << (immediate ? "true" : "false") << std::endl;
    }

    void setFanSpeed(int rpm) {
        std::cout << "[CoolingTower] 风扇转速 " << rpm << "RPM" << std::endl;
    }
};

#endif
