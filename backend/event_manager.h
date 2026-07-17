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

    // 单例
    static EventManager& instance();
    static void setInstance(EventManager* mgr);

    // 构造函数：注入依赖
    // notifyCb 可选 — 不传则默认走 SocketServer（向后兼容）
    EventManager(ConfigManager& configMgr, LinkageEngine& linkageEng);
    EventManager(ConfigManager& configMgr, LinkageEngine& linkageEng,
                 NotifyCallback notifyCb);

    ~EventManager() {}

    EventManager(const EventManager&) = delete;
    EventManager& operator=(const EventManager&) = delete;

    // 一体模式下注入前端通知回调
    void setNotifyCallback(NotifyCallback cb) { notifyCb_ = cb; }
    void processAddEvent(const Event& event);

    // 处理告警消除（设备事件）
    void processClearEvent(const std::string& deviceName, int frameID, const std::string& alarmField);
    // 处理告警消除（按 EventId 精准匹配，系统事件用）
    void processClearEvent(const EventId& eventId);

    // ========= 查询接口 =========

    // 按 EventId 查找事件，不存在返回 NULL
    const Event* findEvent(const EventId& id) const;

    // 当前活跃事件数量
    size_t activeCount() const;

    // 获取所有当前活跃事件
    std::vector<Event> getActiveEvents() const;

    // 按 deviceName 查找该设备的所有活跃事件（用于断联批量处理等）
    std::vector<Event> findEventsByDeviceName(const std::string& deviceName) const;

private:
    // 生成 EventId
    static EventId makeEventId(const std::string& deviceName, int frameID, const std::string& alarmField);

    // 通知前端（通过注入的回调；若未注入则走 SocketServer 桩）
    // isActive: true=alarm_active, false=alarm_cleared
    void notifyFrontend(const Event& event, bool isActive);

    // 写日志（通过 LogWriter 桩）
    void writeLog(const Event& event, const char* action);

    // 核心存储：O(1) 查找
    std::unordered_map<EventId, Event> activeEvents_;

    ConfigManager& configMgr_;
    LinkageEngine& linkageEng_;
    NotifyCallback notifyCb_;   // 为空时默认走 SocketServer

    mutable QMutex mutex_;  // 保护 activeEvents_ 并发访问

    static EventManager* instance_;
};

#endif // EVENT_MANAGER_H
