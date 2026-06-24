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
    ExternalAPI(EventManager& eventMgr, ConfigManager& configMgr);

    // ========= 工厂方法 =========

    Event createAlarm(int protocolID, int frameID,
                      const std::string& alarmField,
                      EventLevel level,
                      const std::string& description);

    // ========= 入口方法 =========

    void addEvent(const Event& event);
    void clearEvent(int protocolID, int frameID, const std::string& alarmField);

    // ========= 查询方法 =========

    // 获取全量报警目录（含降级/屏蔽状态），供前端配置页面使用
    std::vector<AlarmDef> getAlarmCatalog() const;

private:
    EventManager& eventMgr_;
    ConfigManager& configMgr_;
};

#endif // EXTERNAL_API_H
