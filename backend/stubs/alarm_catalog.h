#ifndef ALARM_CATALOG_H
#define ALARM_CATALOG_H

#include "../event_types.h"
#include <vector>

// ============================================================
// 桩模块：报警定义目录
// 模拟项目配置模块提供的所有下位机/状态帧/报警字段定义
// 实际项目中由配置模块提供，本需求仅做桩
// ============================================================
class AlarmCatalog {
public:
    // 返回所有报警定义（不含降级/屏蔽状态，仅静态定义）
    static std::vector<AlarmDef> getAllDefinitions();
};

#endif
