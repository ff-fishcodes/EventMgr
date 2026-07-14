#ifndef EVENT_MANAGER_H
#define EVENT_MANAGER_H

#include "event_types.h"
#include <functional>
#include <unordered_map>
#include <QMutex>

class ConfigManager;
class LinkageEngine;

// ============================================================
// EventManager — 事件管理中心（线程安全）
// 核心数据存储，管理所有活跃事件的增删查
// ============================================================
class EventManager {
public:
    using NotifyCallback = std::function<void(const std::string& json)>;

    EventManager(ConfigManager& configMgr, LinkageEngine& linkageEng);
    EventManager(ConfigManager& configMgr, LinkageEngine& linkageEng,
                 NotifyCallback notifyCb);

    ~EventManager() {}

    void processAddEvent(const Event& event);
    void processClearEvent(int protocolID, int frameID, const std::string& alarmField);
    void processClearEvent(const EventId& eventId);

    const Event* findEvent(const EventId& id) const;
    size_t activeCount() const;
    std::vector<Event> getActiveEvents() const;
    std::vector<Event> findEventsByProtocolID(int protocolID) const;

private:
    static EventId makeEventId(int protocolID, int frameID, const std::string& alarmField);
    void notifyFrontend(const Event& event, bool isActive);
    void writeLog(const Event& event, const char* action);

    std::unordered_map<EventId, Event> activeEvents_;

    ConfigManager& configMgr_;
    LinkageEngine& linkageEng_;
    NotifyCallback notifyCb_;

    mutable QMutex mutex_;  // 保护 activeEvents_ 并发访问
};

#endif // EVENT_MANAGER_H
