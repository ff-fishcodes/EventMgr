#include "external_api.h"
#include "event_manager.h"
#include "config_manager.h"
#include "stubs/alarm_catalog.h"
#include "stubs/log_writer.h"
#include "system_events.h"
#include <sstream>
#include <cstdlib>

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

    // 目录中找不到 → 用默认值（提示等级 + 字段名作描述），并记录日志
    {
        std::ostringstream warn;
        warn << "事件不在报警目录中: " << targetId << "，使用默认等级(提示)";
        LogWriter::write(warn.str());
    }
    Event event = createAlarm(protocolID, frameID, alarmField,
                               EventLevel::Info, alarmField);
    addEvent(event);
}

void ExternalAPI::addEvent(const Event& event) {
    eventMgr_.processAddEvent(event);
}

void ExternalAPI::triggerSystemEvent(const std::string& eventName, bool isActive) {
    const SystemEventDef* def = findSystemEventDef(eventName);
    if (!def) {
        std::ostringstream warn;
        warn << "未注册的系统事件名: " << eventName << "，忽略";
        LogWriter::write(warn.str());
        return;
    }

    if (!isActive) {
        clearEvent(eventName);
        return;
    }

    Event event;
    event.source        = EventSource::System;
    event.id            = eventName;
    event.alarmField    = eventName;
    event.description   = def->description;
    event.originalLevel = def->level;
    event.effectiveLevel = def->level;
    event.state         = EventState::Active;

    addEvent(event);
}

void ExternalAPI::triggerSystemEvent(const std::string& eventName,
                                       int protocolID, bool isActive) {
    const SystemEventDef* def = findSystemEventDef(eventName);
    if (!def) {
        std::ostringstream warn;
        warn << "未注册的系统事件名: " << eventName << "，忽略";
        LogWriter::write(warn.str());
        return;
    }

    std::ostringstream idStr;
    idStr << eventName << ":" << protocolID;
    std::string fullId = idStr.str();

    if (!isActive) {
        clearEvent(fullId);
        return;
    }

    Event event;
    event.source        = EventSource::System;
    event.id            = fullId;
    event.protocolID    = protocolID;
    event.alarmField    = eventName;
    event.description   = "下位机" + std::to_string(protocolID) + "-" + def->description;
    event.originalLevel = def->level;
    event.effectiveLevel = def->level;
    event.state         = EventState::Active;

    addEvent(event);
}

void ExternalAPI::clearEvent(int protocolID, int frameID,
                              const std::string& alarmField) {
    eventMgr_.processClearEvent(protocolID, frameID, alarmField);
}

void ExternalAPI::clearEvent(const std::string& eventId) {
    // 尝试按 Device 格式 "P-F-A" 解析（含两个 '-'）
    size_t d1 = eventId.find('-');
    size_t d2 = eventId.find('-', d1 + 1);
    if (d1 != std::string::npos && d2 != std::string::npos) {
        int pid = std::atoi(eventId.substr(0, d1).c_str());
        int fid = std::atoi(eventId.substr(d1 + 1, d2 - d1 - 1).c_str());
        std::string field = eventId.substr(d2 + 1);
        eventMgr_.processClearEvent(pid, fid, field);
    } else {
        // System 事件（如 "comm_lost"、"comm_lost:3"），直接 ID 匹配
        eventMgr_.processClearEvent(eventId);
    }
}

std::vector<Event> ExternalAPI::getActiveEvents() const {
    return eventMgr_.getActiveEvents();
}

std::vector<AlarmDef> ExternalAPI::getAlarmCatalog() const {
    // 1. 设备报警定义（桩）
    std::vector<AlarmDef> defs = AlarmCatalog::getAllDefinitions();

    // 2. 系统事件定义也纳入目录
    const std::vector<SystemEventDef>& sysDefs = getSystemEventDefs();
    for (std::vector<SystemEventDef>::const_iterator sit = sysDefs.begin();
         sit != sysDefs.end(); ++sit) {
        AlarmDef d;
        d.id            = sit->name;
        d.description   = sit->description;
        d.originalLevel = sit->level;
        defs.push_back(d);
    }

    // 3. 合并 ConfigManager 当前的降级/屏蔽状态
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
