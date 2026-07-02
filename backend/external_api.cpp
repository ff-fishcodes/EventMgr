#include "external_api.h"
#include "event_manager.h"
#include "config_manager.h"
#include "stubs/alarm_catalog.h"
#include <sstream>

ExternalAPI::ExternalAPI(EventManager& eventMgr, ConfigManager& configMgr)
    : eventMgr_(eventMgr), configMgr_(configMgr) {}

Event ExternalAPI::createAlarm(int protocolID, int frameID,
                                const std::string& alarmField,
                                EventLevel level,
                                const std::string& description) {
    Event event;
    event.protocolID    = protocolID;
    event.frameID       = frameID;
    event.alarmField    = alarmField;
    event.description   = description;
    event.originalLevel = level;
    event.effectiveLevel = level;
    event.state         = EventState::Active;

    std::ostringstream oss;
    oss << protocolID << "-" << frameID << "-" << alarmField;
    event.id = oss.str();

    return event;
}

void ExternalAPI::triggerAlarm(int protocolID, int frameID,
                                 const std::string& alarmField, bool isActive) {
    if (!isActive) {
        clearEvent(protocolID, frameID, alarmField);
        return;
    }

    // 构造 EventId 用于查目录
    std::ostringstream idStr;
    idStr << protocolID << "-" << frameID << "-" << alarmField;
    std::string targetId = idStr.str();

    // 从报警目录查定义（等级和描述由配置模块提供，observe 不需要知道）
    std::vector<AlarmDef> defs = AlarmCatalog::getAllDefinitions();
    for (std::vector<AlarmDef>::const_iterator it = defs.begin();
         it != defs.end(); ++it) {
        if (it->id == targetId) {
            Event event = createAlarm(protocolID, frameID, alarmField,
                                       it->originalLevel, it->description);
            addEvent(event);
            return;
        }
    }

    // 目录中找不到 → 用默认值（提示等级 + 字段名作描述）
    Event event = createAlarm(protocolID, frameID, alarmField,
                               EventLevel::Info, alarmField);
    addEvent(event);
}

void ExternalAPI::addEvent(const Event& event) {
    eventMgr_.processAddEvent(event);
}

void ExternalAPI::clearEvent(int protocolID, int frameID,
                              const std::string& alarmField) {
    eventMgr_.processClearEvent(protocolID, frameID, alarmField);
}

std::vector<Event> ExternalAPI::getActiveEvents() const {
    return eventMgr_.getActiveEvents();
}

std::vector<AlarmDef> ExternalAPI::getAlarmCatalog() const {
    // 1. 从项目配置模块获取所有报警静态定义（桩）
    std::vector<AlarmDef> defs = AlarmCatalog::getAllDefinitions();

    // 2. 合并 ConfigManager 当前的降级/屏蔽状态
    for (std::vector<AlarmDef>::iterator it = defs.begin();
         it != defs.end(); ++it) {
        if (configMgr_.hasDowngrade(it->id)) {
            it->downgraded = true;
            it->downgradeTo = configMgr_.getEffectiveLevel(it->id, it->originalLevel);
        }
        it->shielded = configMgr_.isShielded(it->id);
    }

    return defs;
}
