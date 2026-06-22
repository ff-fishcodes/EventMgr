#include "log_writer.h"
#include <iostream>

void LogWriter::write(const std::string& message) {
    // 桩实现：打印到标准输出，实际项目中写入日志文件/系统
    std::cout << "[LogWriter] " << message << std::endl;
}
