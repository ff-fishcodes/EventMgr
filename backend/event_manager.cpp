#include "event_manager.h"
#include "config_manager.h"
#include "linkage_engine.h"
#include "stubs/socket_server.h"
#include "stubs/log_writer.h"
#include <sstream>
#include <ctime>

// ============================================================
// 构造 / 辅助
// ============================================================

EventManager::EventManager(ConfigManager& configMgr, LinkageEngine& linkageEng)
    : configMgr_(configMgr), linkageEng_(linkageEng) {}

EventManager::EventManager(ConfigManager& configMgr, LinkageEngine& linkageEng,
                           NotifyCallback notifyCb)
    : configMgr_(configMgr), linkageEng_(linkageEng), notifyCb_(notifyCb) {}

EventId EventManager::makeEventId(int protocolID, int frameID,
                                   const std::string& alarmField) {
    std::ostringstream oss;
    oss << protocolID << "-" << frameID << "-" << alarmField;
    return oss.str();
}

// ============================================================
// 核心处理
// ============================================================

void EventManager::processAddEvent(const Event& event) {
    std::unordered_map<EventId, Event>::iterator it = activeEvents_.find(event.id);
    if (it != activeEvents_.end()) {
        return;  // 重复上报
    }

    Event stored = event;
    stored.effectiveLevel = configMgr_.getEffectiveLevel(event.id, event.originalLevel);
    stored.state = EventState::Active;

    // 记录事件产生时间
    {
        std::time_t now = std::time(NULL);
        char buf[20];
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
        stored.timestamp = buf;
    }

    activeEvents_[stored.id] = stored;

    // 触发联动（引擎内部查配置表+等级默认）
    linkageEng_.executeActive(stored);

    // 通知前端（未被屏蔽时）
    if (!configMgr_.isShielded(event.id)) {
        notifyFrontend(stored, true);
    }

    writeLog(stored, "发生");
}

void EventManager::processClearEvent(int protocolID, int frameID,
                                      const std::string& alarmField) {
    EventId id = makeEventId(protocolID, frameID, alarmField);

    std::unordered_map<EventId, Event>::iterator it = activeEvents_.find(id);
    if (it == activeEvents_.end()) {
        return;
    }

    Event& event = it->second;
    event.state = EventState::Cleared;

    linkageEng_.executeCleared(event);

    notifyFrontend(event, false);

    writeLog(event, "消除");

    activeEvents_.erase(it);
}

// ============================================================
// 查询接口
// ============================================================

const Event* EventManager::findEvent(const EventId& id) const {
    std::unordered_map<EventId, Event>::const_iterator it = activeEvents_.find(id);
    if (it != activeEvents_.end()) {
        return &(it->second);
    }
    return NULL;
}

std::vector<Event> EventManager::getActiveEvents() const {
    std::vector<Event> result;
    for (std::unordered_map<EventId, Event>::const_iterator it = activeEvents_.begin();
         it != activeEvents_.end(); ++it) {
        result.push_back(it->second);
    }
    return result;
}

size_t EventManager::activeCount() const {
    return activeEvents_.size();
}

std::vector<Event> EventManager::findEventsByProtocolID(int protocolID) const {
    std::vector<Event> result;
    for (std::unordered_map<EventId, Event>::const_iterator it = activeEvents_.begin();
         it != activeEvents_.end(); ++it) {
        if (it->second.protocolID == protocolID) {
            result.push_back(it->second);
        }
    }
    return result;
}

// ============================================================
// 通知与日志
// ============================================================

void EventManager::notifyFrontend(const Event& event, bool isActive) {
    const char* alarmType = isActive ? "alarm_active" : "alarm_cleared";

    std::ostringstream json;
    json << "{"
         << "\"type\":\"" << alarmType << "\","
         << "\"protocolID\":" << event.protocolID << ","
         << "\"frameID\":" << event.frameID << ","
         << "\"alarmField\":\"" << event.alarmField << "\","
         << "\"description\":\"" << event.description << "\","
         << "\"level\":" << static_cast<int>(event.effectiveLevel)
         << "}";

    std::string jsonStr = json.str();
    if (notifyCb_) {
        notifyCb_(jsonStr);
    } else {
        SocketServer::notifyFrontend(jsonStr);
    }
}

void EventManager::writeLog(const Event& event, const char* action) {
    std::ostringstream msg;
    msg << "下位机" << event.protocolID
        << "-" << event.description
        << "-" << action;
    LogWriter::write(msg.str());
}
