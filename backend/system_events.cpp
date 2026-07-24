#include "system_events.h"
#include <sstream>

// ============================================================
// 统一报警定义注册表
// ============================================================
static std::vector<AlarmDef> g_alarmDefs;

static std::string makeId(const std::string& deviceName, int frameID,
                           const std::string& alarmField) {
    std::ostringstream oss;
    oss << deviceName << "-" << frameID << "-" << alarmField;
    return oss.str();
}

void registerAlarmDef(const std::string& deviceName, int frameID,
                      const std::string& alarmField,
                      const std::string& description,
                      EventLevel level, bool isSystem) {
    std::string id = makeId(deviceName, frameID, alarmField);

    // 同名覆盖
    for (std::vector<AlarmDef>::iterator it = g_alarmDefs.begin();
         it != g_alarmDefs.end(); ++it) {
        if (it->id == id) {
            it->description   = description;
            it->originalLevel = level;
            it->isSystem      = isSystem;
            return;
        }
    }
    AlarmDef d;
    d.id            = id;
    d.description   = description;
    d.originalLevel = level;
    d.isSystem      = isSystem;
    g_alarmDefs.push_back(d);
}

const AlarmDef* findAlarmDef(const std::string& eventId) {
    for (std::vector<AlarmDef>::const_iterator it = g_alarmDefs.begin();
         it != g_alarmDefs.end(); ++it) {
        if (it->id == eventId) return &(*it);
    }
    return NULL;
}

const std::vector<AlarmDef>& getRegisteredAlarmDefs() {
    return g_alarmDefs;
}

void clearRegisteredAlarmDefs() {
    g_alarmDefs.clear();
}
