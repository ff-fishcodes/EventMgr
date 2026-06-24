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

void ExternalAPI::addEvent(const Event& event) {
    eventMgr_.processAddEvent(event);
}

void ExternalAPI::clearEvent(int protocolID, int frameID,
                              const std::string& alarmField) {
    eventMgr_.processClearEvent(protocolID, frameID, alarmField);
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
