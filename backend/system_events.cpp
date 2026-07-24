#include "system_events.h"
#include <sstream>

// ============================================================
// 系统事件定义表（外部可配置）
// 初始为空，由业务代码通过 addSystemEventDef 注册
// 内部直接存储 AlarmDef，isSystem=true，id="模块名-0-事件名"
// ============================================================
static std::vector<AlarmDef> g_systemEvents;

void addSystemEventDef(const std::string& moduleName, const std::string& name,
                       const std::string& description, EventLevel level) {
    std::ostringstream idStr;
    idStr << moduleName << "-0-" << name;

    // 同模块+同名覆盖
    for (std::vector<AlarmDef>::iterator it = g_systemEvents.begin();
         it != g_systemEvents.end(); ++it) {
        if (it->id == idStr.str()) {
            it->description    = description;
            it->originalLevel  = level;
            return;
        }
    }
    AlarmDef d;
    d.id            = idStr.str();
    d.description   = description;
    d.originalLevel = level;
    d.isSystem      = true;
    g_systemEvents.push_back(d);
}

const std::vector<AlarmDef>& getSystemEventDefs() {
    return g_systemEvents;
}

const AlarmDef* findSystemEventDef(const std::string& moduleName,
                                    const std::string& name) {
    std::ostringstream idStr;
    idStr << moduleName << "-0-" << name;
    std::string targetId = idStr.str();

    for (std::vector<AlarmDef>::const_iterator it = g_systemEvents.begin();
         it != g_systemEvents.end(); ++it) {
        if (it->id == targetId) {
            return &(*it);
        }
    }
    return NULL;
}

void clearSystemEventDefs() {
    g_systemEvents.clear();
}
