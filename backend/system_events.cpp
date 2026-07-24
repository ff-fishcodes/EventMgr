#include "system_events.h"

// ============================================================
// 系统事件定义表（外部可配置）
// 初始为空，由业务代码通过 addSystemEventDef 注册
// ============================================================
static std::vector<SystemEventDef> g_systemEvents;

void addSystemEventDef(const std::string& moduleName, const std::string& name,
                       const std::string& description, EventLevel level) {
    // 同模块+同名覆盖，避免重复
    for (std::vector<SystemEventDef>::iterator it = g_systemEvents.begin();
         it != g_systemEvents.end(); ++it) {
        if (it->moduleName == moduleName && it->name == name) {
            it->description = description;
            it->level = level;
            return;
        }
    }
    g_systemEvents.push_back(SystemEventDef(moduleName, name, description, level));
}

const std::vector<SystemEventDef>& getSystemEventDefs() {
    return g_systemEvents;
}

const SystemEventDef* findSystemEventDef(const std::string& moduleName,
                                          const std::string& name) {
    for (std::vector<SystemEventDef>::const_iterator it = g_systemEvents.begin();
         it != g_systemEvents.end(); ++it) {
        if (it->moduleName == moduleName && it->name == name) {
            return &(*it);
        }
    }
    return NULL;
}

void clearSystemEventDefs() {
    g_systemEvents.clear();
}
