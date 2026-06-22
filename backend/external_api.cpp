#include "external_api.h"
#include "event_manager.h"
#include <sstream>

ExternalAPI::ExternalAPI(EventManager& eventMgr)
    : eventMgr_(eventMgr) {}

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
    event.effectiveLevel = level;  // 初始与原始一致，addEvent 时可能被降级覆盖
    event.state         = EventState::Active;

    // 生成事件编号
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
