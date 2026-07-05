#ifndef SYSTEM_EVENTS_H
#define SYSTEM_EVENTS_H

#include "event_types.h"
#include <vector>

// ============================================================
// 系统事件预定义列表
//
// 后端自行监测产生的事件必须在此集中定义，业务代码通过
// ExternalAPI::triggerSystemEvent() 触发，只能使用已注册的事件名。
//
// 具体事件内容由项目方自行增删，此处仅提供示例。
// ============================================================

// 获取系统事件预定义列表
const std::vector<SystemEventDef>& getSystemEventDefs();

// 按名称查找系统事件定义，未找到时返回 NULL
const SystemEventDef* findSystemEventDef(const std::string& name);

#endif // SYSTEM_EVENTS_H
