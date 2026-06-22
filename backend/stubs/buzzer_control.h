#ifndef BUZZER_CONTROL_H
#define BUZZER_CONTROL_H

#include <string>

// ============================================================
// 桩模块：BuzzerControl — 蜂鸣器控制（预留）
// 属项目已有基础设施，当前可能尚未实现，预留接口
// ============================================================
class BuzzerControl {
public:
    // 播放指定模式的蜂鸣
    // target: 蜂鸣模式名
    // param:  附加参数（时长、次数等）
    static void play(const std::string& target, const std::string& param);
};

#endif // BUZZER_CONTROL_H
