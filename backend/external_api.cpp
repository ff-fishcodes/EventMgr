#include "external_api.h"
#include "event_manager.h"
#include "config_manager.h"
#include "linkage_engine.h"
#include "stubs/log_writer.h"
#include "system_events.h"
#include <sstream>
#include <cstdlib>
#include <utility>

ExternalAPI* ExternalAPI::instance_ = NULL;

ExternalAPI& ExternalAPI::instance() { return *instance_; }
void ExternalAPI::setInstance(ExternalAPI* api) { instance_ = api; }

ExternalAPI::ExternalAPI(EventManager& eventMgr, ConfigManager& configMgr)
    : eventMgr_(eventMgr), configMgr_(configMgr) {}

Event ExternalAPI::createAlarm(const std::string& deviceName, int frameID,
                                const std::string& alarmField,
                                EventLevel level,
                                const std::string& description) {
    Event event;
    event.deviceName    = deviceName;
    event.frameID       = frameID;
    event.alarmField    = alarmField;
    event.description   = description;
    event.originalLevel = level;
    event.effectiveLevel = level;
    event.state         = EventState::Active;

    std::ostringstream oss;
    oss << deviceName << "-" << frameID << "-" << alarmField;
    event.id = oss.str();

    return event;
}

void ExternalAPI::triggerAlarm(const std::string& deviceName, int frameID,
                                 const std::string& alarmField, bool isActive,
                                 EventLevel fallbackLevel) {
    if (!isActive) {
        clearEvent(deviceName, frameID, alarmField);
        return;
    }

    std::ostringstream idStr;
    idStr << deviceName << "-" << frameID << "-" << alarmField;
    std::string targetId = idStr.str();

    // 统一查已注册的报警定义（设备+系统）
    const AlarmDef* def = findAlarmDef(targetId);
    if (def) {
        Event event = createAlarm(deviceName, frameID, alarmField,
                                  def->originalLevel, def->description);
        event.source = def->isSystem ? EventSource::System : EventSource::Device;
        addEvent(event);
        return;
    }

    // 未定义 → 用 fallbackLevel，标记 Unknown，记日志
    {
        std::ostringstream warn;
        warn << "事件未在目录/系统事件中定义: " << targetId
             << "，使用等级=" << static_cast<int>(fallbackLevel);
        LogWriter::write(warn.str());
    }
    Event fallback = createAlarm(deviceName, frameID, alarmField, fallbackLevel, alarmField);
    fallback.source = EventSource::Unknown;
    addEvent(fallback);
}

void ExternalAPI::addEvent(const Event& event) {
    eventMgr_.processAddEvent(event);
}

void ExternalAPI::registerAlarmDef(const std::string& deviceName, int frameID,
                                     const std::string& alarmField,
                                     const std::string& description,
                                     EventLevel level, bool isSystem) {
    ::registerAlarmDef(deviceName, frameID, alarmField, description, level, isSystem);
}

void ExternalAPI::registerAction(const std::string& name,
                                  const std::string& displayName,
                                  std::function<void()> callback) {
    LinkageEngine::instance().registerAction(name, displayName, std::move(callback));
}

void ExternalAPI::configureEvent(const std::string& eventId,
                                  const std::vector<std::string>& activeNames,
                                  const std::vector<std::string>& clearNames) {
    LinkageEngine::instance().configureEvent(eventId, activeNames, clearNames);
}

void ExternalAPI::setLevelDefault(EventLevel level,
                                   const std::vector<std::string>& activeActions,
                                   const std::vector<std::string>& clearActions) {
    LinkageEngine::instance().setLevelDefault(level, activeActions, clearActions);
}

void ExternalAPI::clearEvent(const std::string& deviceName, int frameID,
                              const std::string& alarmField) {
    eventMgr_.processClearEvent(deviceName, frameID, alarmField);
}

void ExternalAPI::clearEvent(const std::string& eventId) {
    // 尝试按 "deviceName-frameID-alarmField" 格式解析（含两个 '-'）
    size_t d1 = eventId.find('-');
    size_t d2 = eventId.find('-', d1 + 1);
    if (d1 != std::string::npos && d2 != std::string::npos) {
        std::string devName = eventId.substr(0, d1);
        int fid = std::atoi(eventId.substr(d1 + 1, d2 - d1 - 1).c_str());
        std::string field = eventId.substr(d2 + 1);
        eventMgr_.processClearEvent(devName, fid, field);
    } else {
        // 纯系统事件名，直接 ID 匹配
        eventMgr_.processClearEvent(eventId);
    }
}

std::vector<Event> ExternalAPI::getActiveEvents() const {
    return eventMgr_.getActiveEvents();
}

std::vector<AlarmDef> ExternalAPI::getAlarmCatalog() const {
    std::vector<AlarmDef> defs = getRegisteredAlarmDefs();

    // 合并 ConfigManager 当前的降级/屏蔽状态
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
