#include "system_events.h"

// ============================================================
// 系统事件预定义列表（示例）
// 项目方按实际需求自行增删
// ============================================================
static const std::vector<SystemEventDef> kSystemEvents = {
    {"comm_lost",    "下位机通信断连",  EventLevel::Emergency},
    {"comm_restore", "下位机通信恢复",  EventLevel::Info},
    {"disk_full",    "磁盘空间不足",    EventLevel::Serious},
    {"cpu_overload", "CPU 过载",        EventLevel::Serious},
    {"service_error", "关键服务异常",   EventLevel::Emergency},
};

const std::vector<SystemEventDef>& getSystemEventDefs() {
    return kSystemEvents;
}

const SystemEventDef* findSystemEventDef(const std::string& name) {
    for (std::vector<SystemEventDef>::const_iterator it = kSystemEvents.begin();
         it != kSystemEvents.end(); ++it) {
        if (it->name == name) {
            return &(*it);
        }
    }
    return NULL;
}
