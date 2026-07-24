#ifndef SYSTEM_EVENTS_H
#define SYSTEM_EVENTS_H

#include "event_types.h"
#include <vector>

// ============================================================
// 系统事件定义表
//
// 后端自行监测产生的事件（通信断连、磁盘满等）在此登记。
// 由外部业务代码在启动阶段通过 ExternalAPI::addSystemEventDef() 注册，
// 内部直接存储为 AlarmDef（isSystem=true，id="模块名-0-事件名"）。
//
// 线程安全说明：注册在启动阶段完成，查询在运行阶段进行，
// 假定所有注册先于任何触发发生，无需额外加锁。
// ============================================================

// 注册一条系统事件定义（启动阶段调用，内部构造 AlarmDef）
void addSystemEventDef(const std::string& moduleName, const std::string& name,
                       const std::string& description, EventLevel level);

// 获取当前已注册的系统事件定义列表
const std::vector<AlarmDef>& getSystemEventDefs();

// 按模块名+事件名查找系统事件定义，未找到时返回 NULL
const AlarmDef* findSystemEventDef(const std::string& moduleName,
                                    const std::string& name);

// 清空所有系统事件定义（测试/重启用）
void clearSystemEventDefs();

#endif // SYSTEM_EVENTS_H
