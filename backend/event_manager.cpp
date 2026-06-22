#include "event_manager.h"
#include "config_manager.h"
#include "linkage_engine.h"
#include "stubs/socket_server.h"
#include "stubs/log_writer.h"
#include <sstream>

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
    // 1. 查重：已存在则忽略
    std::unordered_map<EventId, Event>::iterator it = activeEvents_.find(event.id);
    if (it != activeEvents_.end()) {
        return;  // 重复上报，直接返回
    }

    // 2. 创建事件副本，计算 effectiveLevel
    Event stored = event;
    stored.effectiveLevel = configMgr_.getEffectiveLevel(event.id, event.originalLevel);
    stored.state = EventState::Active;

    // 3. 存入活跃事件表
    activeEvents_[stored.id] = stored;

    // 4. 触发联动
    linkageEng_.executeActive(stored);

    // 5. 通知前端（未被屏蔽时）
    if (!configMgr_.isShielded(event.id)) {
        notifyFrontend(stored, true);   // alarm_active
    }

    // 6. 写日志
    writeLog(stored, "发生");
}

void EventManager::processClearEvent(int protocolID, int frameID,
                                      const std::string& alarmField) {
    EventId id = makeEventId(protocolID, frameID, alarmField);

    // 1. 查找
    std::unordered_map<EventId, Event>::iterator it = activeEvents_.find(id);
    if (it == activeEvents_.end()) {
        return;  // 事件不存在，直接返回
    }

    Event& event = it->second;
    event.state = EventState::Cleared;

    // 2. 触发联动（消除侧）
    linkageEng_.executeCleared(event);

    // 3. 通知前端移除
    notifyFrontend(event, false);  // alarm_cleared

    // 4. 写日志
    writeLog(event, "消除");

    // 5. 从活跃表移除
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
// 内部辅助：通知与日志
// ============================================================

void EventManager::notifyFrontend(const Event& event, bool isActive) {
    const char* alarmType = isActive ? "alarm_active" : "alarm_cleared";
    const std::vector<LinkageAction>& actions =
        isActive ? event.activeActions : event.clearActions;

    std::ostringstream json;
    json << "{"
         << "\"type\":\"" << alarmType << "\","
         << "\"protocolID\":" << event.protocolID << ","
         << "\"frameID\":" << event.frameID << ","
         << "\"alarmField\":\"" << event.alarmField << "\","
         << "\"description\":\"" << event.description << "\","
         << "\"level\":" << static_cast<int>(event.effectiveLevel) << ",";

    // 序列化 uiActions
    json << "\"uiActions\":[";
    bool first = true;
    for (std::vector<LinkageAction>::const_iterator it = actions.begin();
         it != actions.end(); ++it) {
        if (it->type == LinkageAction::LockUI || it->type == LinkageAction::UnlockUI) {
            if (!first) json << ",";
            first = false;
            const char* actStr = (it->type == LinkageAction::LockUI) ? "lock_ui" : "unlock_ui";
            json << "{\"action\":\"" << actStr << "\",\"target\":\"" << it->target << "\"}";
        }
    }
    json << "]";
    json << "}";

    // 优先走注入的回调，未注入则走 SocketServer（向后兼容）
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
