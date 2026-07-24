#ifndef EXTERNAL_API_H
#define EXTERNAL_API_H

#include "event_types.h"
#include <vector>
#include <functional>

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

    Event createAlarm(const std::string& deviceName, int frameID,
                      const std::string& alarmField,
                      EventLevel level,
                      const std::string& description);

    // observe 组件对接：报警位变化时调用
    // 等级和描述按 EventId 查报警目录，再按 alarmField 查系统事件定义；
    // 两级都找不到时用传入的 fallbackLevel（默认提示），描述用 alarmField。
    // isActive=true → 产生事件，isActive=false → 消除事件
    void triggerAlarm(const std::string& deviceName, int frameID,
                      const std::string& alarmField, bool isActive,
                      EventLevel fallbackLevel = EventLevel::Info);

    // ========= 报警定义注册（启动阶段配置）=========

    // 统一注册设备事件或系统事件
    void registerAlarmDef(const std::string& deviceName, int frameID,
                          const std::string& alarmField,
                          const std::string& description,
                          EventLevel level, bool isSystem = false);

    // ========= 联动配置门面（业务代码不直接依赖 LinkageEngine）=========

    void registerAction(const std::string& name, const std::string& displayName,
                        std::function<void()> callback);
    void configureEvent(const std::string& eventId,
                        const std::vector<std::string>& activeNames,
                        const std::vector<std::string>& clearNames);
    void setLevelDefault(EventLevel level,
                         const std::vector<std::string>& activeActions,
                         const std::vector<std::string>& clearActions);

    void addEvent(const Event& event);
    void clearEvent(const std::string& deviceName, int frameID, const std::string& alarmField);
    void clearEvent(const std::string& eventId);  // 按 EventId 直接清除

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
