#ifndef LOG_WRITER_H
#define LOG_WRITER_H

#include <string>

// ============================================================
// 桩模块：LogWriter — 日志写入
// 属项目已有基础设施，本需求不实现，仅提供接口声明
// ============================================================
class LogWriter {
public:
    // 写入日志
    static void write(const std::string& message);
};

#endif // LOG_WRITER_H
