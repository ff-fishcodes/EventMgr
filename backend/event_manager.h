#ifndef EVENT_MANAGER_H
#define EVENT_MANAGER_H

#include "event_types.h"
#include <functional>
#include <unordered_map>

class ConfigManager;
class LinkageEngine;

// ============================================================
// EventManager — 事件管理中心
// 核心数据存储，管理所有活跃事件的增删查
// ============================================================
class EventManager {
public:
    // 前端通知回调: 接收 JSON 字符串
    //   - 分离模式: 回调 = SocketServer::notifyFrontend
    //   - 一体模式: 回调 = 直接更新 Qt 事件列表
    using NotifyCallback = std::function<void(const std::string& json)>;

    // 构造函数：注入依赖
    // notifyCb 可选 — 不传则默认走 SocketServer（向后兼容）
    EventManager(ConfigManager& configMgr, LinkageEngine& linkageEng);
    EventManager(ConfigManager& configMgr, LinkageEngine& linkageEng,
                 NotifyCallback notifyCb);

    ~EventManager() {}

    // 处理告警产生
    void processAddEvent(const Event& event);

    // 处理告警消除
    void processClearEvent(int protocolID, int frameID, const std::string& alarmField);

    // ========= 查询接口 =========

    // 按 EventId 查找事件，不存在返回 NULL
    const Event* findEvent(const EventId& id) const;

    // 当前活跃事件数量
    size_t activeCount() const;

    // 按 protocolID 查找该下位机的所有活跃事件（用于断联批量处理等）
    std::vector<Event> findEventsByProtocolID(int protocolID) const;

private:
    // 生成 EventId
    static EventId makeEventId(int protocolID, int frameID, const std::string& alarmField);

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
};

#endif // EVENT_MANAGER_H
