#ifndef EXTERNAL_API_H
#define EXTERNAL_API_H

#include "event_types.h"
#include <vector>

class EventManager;
class ConfigManager;

// ============================================================
// ExternalAPI — 对外接口层（本需求的唯一门面）
//
// 职责:
//   - 工厂方法: createAlarm() 创建事件值对象
//   - 入口方法: addEvent() / clearEvent()
//   - 查询方法: getAlarmCatalog() 获取全量报警定义及配置状态
//
// 所有外部模块（NetworkReceiver、通信监测等）仅通过此类与本需求交互
// ============================================================
class ExternalAPI {
public:
    // 单例
    static ExternalAPI& instance();
    static void setInstance(ExternalAPI* api);

    ExternalAPI(EventManager& eventMgr, ConfigManager& configMgr);
    ~ExternalAPI() {}

    ExternalAPI(const ExternalAPI&) = delete;
    ExternalAPI& operator=(const ExternalAPI&) = delete;

    // ========= 工厂方法 =========

    Event createAlarm(int protocolID, int frameID,
                      const std::string& alarmField,
                      EventLevel level,
                      const std::string& description);

    // observe 组件对接：报警位变化时调用，自动查等级+描述
    // isActive=true → 产生事件，isActive=false → 消除事件
    void triggerAlarm(int protocolID, int frameID,
                      const std::string& alarmField, bool isActive);

    // 系统事件入口（eventName 必须在 system_events.h 中已定义）
    // 纯系统事件（如 disk_full）
    void triggerSystemEvent(const std::string& eventName, bool isActive);
    // 关联某下位机的系统事件（如 comm_lost:3）
    void triggerSystemEvent(const std::string& eventName, int protocolID, bool isActive);

    // ========= 入口方法（可独立使用）=========

    void addEvent(const Event& event);
    void clearEvent(int protocolID, int frameID, const std::string& alarmField);
    void clearEvent(const std::string& eventId);  // 按 EventId 直接清除（系统事件用）

    // ========= 查询方法 =========

    // 获取当前所有活跃事件
    std::vector<Event> getActiveEvents() const;

    // 获取全量报警目录（含降级/屏蔽状态），供前端配置页面使用
    std::vector<AlarmDef> getAlarmCatalog() const;

private:
    EventManager& eventMgr_;
    ConfigManager& configMgr_;
    static ExternalAPI* instance_;
};

#endif // EXTERNAL_API_H
