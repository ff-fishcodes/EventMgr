#ifndef SYSTEM_EVENTS_H
#define SYSTEM_EVENTS_H

#include "event_types.h"
#include <vector>

// ============================================================
// 统一报警定义注册表
//
// 设备事件和系统事件通过同一接口注册，内部存储为 AlarmDef。
// 注册在启动阶段完成，查询在运行阶段进行。
// ============================================================

// 注册一条报警定义（设备或系统事件）
// ID 格式: "deviceName-frameID-alarmField"
void registerAlarmDef(const std::string& deviceName, int frameID,
                      const std::string& alarmField,
                      const std::string& description,
                      EventLevel level, bool isSystem);

// 按完整 EventId 查找，未找到返回 NULL
const AlarmDef* findAlarmDef(const std::string& eventId);

// 获取所有已注册的报警定义
const std::vector<AlarmDef>& getRegisteredAlarmDefs();

// 清空（测试/重启用）
void clearRegisteredAlarmDefs();

#endif // SYSTEM_EVENTS_H
