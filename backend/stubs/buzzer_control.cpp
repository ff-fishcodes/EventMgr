#include "buzzer_control.h"
#include <iostream>

void BuzzerControl::play(const std::string& target, const std::string& param) {
    // 桩实现：打印到标准输出，实际项目中控制蜂鸣器硬件
    std::cout << "[BuzzerControl::play] target=" << target
              << " param=" << param << std::endl;
}
